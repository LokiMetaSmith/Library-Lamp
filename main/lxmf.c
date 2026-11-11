#include "lxmf.h"
#include <string.h>
#include "esp_log.h"
#include "mbedtls/sha256.h"
#include "bulletin_board.h"
#include "lxmf_nomadnet.h"

static const char* TAG = "LXMF";

void lxmf_init(void) {
    ESP_LOGI(TAG, "Initializing Minimal LXMF Layer");
}

bool lxmf_handle_packet(const rns_packet_t* pkt) {
    if (!pkt || pkt->type != RNS_PKT_TYPE_DATA) {
        return false;
    }

    ESP_LOGI(TAG, "Received LXMF packet of length %zu", pkt->payload_len);

    if (pkt->payload_len > 0) {
        const uint8_t* p = pkt->payload;
        size_t len = pkt->payload_len;

        // Minimal msgpack string bounds checking for NomadNet page request
        // Msgpack FixStr (101XXXXX) or str 8 (0xd9) or str 16 (0xda) or str 32 (0xdb)
        const char* req_str = NULL;
        size_t req_len = 0;

        // Try simple byte scanning for the "page " prefix pattern first to avoid full msgpack parsing if not needed
        for (size_t i = 0; i < len - 5; i++) {
            if (p[i] == 'p' && p[i+1] == 'a' && p[i+2] == 'g' && p[i+3] == 'e' && p[i+4] == ' ') {
                req_str = (const char*)(p + i);
                req_len = len - i;
                break;
            }
        }

        if (req_str) {
            char path[128] = {0};
            size_t remaining_len = req_len - 5;
            size_t copy_len = remaining_len < sizeof(path) - 1 ? remaining_len : sizeof(path) - 1;

            strncpy(path, req_str + 5, copy_len); // Extract path
            path[copy_len] = '\0';

            // Clean up trailing non-printable characters or msgpack framing
            for (int i = 0; i < copy_len; i++) {
                if (path[i] < ' ' || path[i] > '~') {
                    path[i] = '\0';
                    break;
                }
            }

            ESP_LOGI(TAG, "Parsed NomadNet Page Request: %s", path);

            char response_buf[230];
            size_t response_len = lxmf_nomadnet_generate_response(path, response_buf, sizeof(response_buf), pkt->recv_interface);

            if (response_len > 0) {
                ESP_LOGI(TAG, "Response generated, ready for TX.");
                extern bool rns_link_send_encrypted_global(const uint8_t* data, size_t len, rns_interface_t ifac);

                if (rns_link_send_encrypted_global((uint8_t*)response_buf, response_len, pkt->recv_interface)) {
                     ESP_LOGI(TAG, "Response sent securely over active link on interface %d", pkt->recv_interface);
                } else {
                    ESP_LOGI(TAG, "No active link, sending response in cleartext on interface %d", pkt->recv_interface);
                    rns_packet_t reply_pkt;
                    memset(&reply_pkt, 0, sizeof(reply_pkt));
                    memcpy(reply_pkt.dest_hash, pkt->dest_hash, 16);
                    reply_pkt.type = RNS_PKT_TYPE_DATA;
                    reply_pkt.flags = RNS_PKT_FLAG_HEADER;
                    reply_pkt.hops = 0;
                    reply_pkt.payload = (uint8_t*)response_buf;
                    reply_pkt.payload_len = response_len;

                    uint8_t out_buf[255];
                    size_t out_len = 0;
                    if (rns_encode_packet(&reply_pkt, out_buf, sizeof(out_buf), &out_len)) {
                        if (pkt->recv_interface == IF_WIFI) {
                            extern void rns_udp_transmit_raw(const uint8_t *data, size_t len);
                            rns_udp_transmit_raw(out_buf, out_len);
                        } else {
                            extern void lora_wan_transmit_raw(const uint8_t *data, size_t len);
                            lora_wan_transmit_raw(out_buf, out_len);
                        }
                    }
                }
            }

            return true;
        } else {
            // Not a page request. Process as a general LXMF bulletin message.
            // Bounds-checked extraction of printable characters
            size_t printable_count = 0;
            for (size_t i = 0; i < len; i++) {
                if (p[i] >= ' ' && p[i] <= '~') printable_count++;
            }

            if (printable_count > (len / 2) && len > 10) {
                ESP_LOGI(TAG, "Sniffed cleartext-like LXMF message, adding to Bulletin Board.");
                char msg_text[300] = {0};
                size_t p_idx = 0;

                // Skip common msgpack header prefixes (very roughly)
                size_t start_idx = 0;
                for (size_t i = 0; i < len && i < 20; i++) {
                     if (p[i] >= 'A' && p[i] <= 'z') { start_idx = i; break; }
                }

                for (size_t i = start_idx; i < len && p_idx < 299; i++) {
                    if (p[i] >= ' ' && p[i] <= '~') {
                        msg_text[p_idx++] = p[i];
                    }
                }
                bb_add_message_local("LoRa/LXMF User", "Message", msg_text, 24);
                return true;
            }
        }
    }

    return false;
}
