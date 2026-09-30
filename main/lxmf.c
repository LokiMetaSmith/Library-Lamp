#include "lxmf.h"
#include <string.h>
#include "esp_log.h"
#include "mbedtls/sha256.h"
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

    // Very naive LXMF msgpack parsing for NomadNet requests
    // A NomadNet request typically looks like `page /` or `page /books/1` in the content

    if (pkt->payload_len > 0) {
        // Find the "page " prefix
        const char* payload_str = (const char*)pkt->payload;
        char* page_req = strnstr(payload_str, "page ", pkt->payload_len);

        if (page_req) {
            char path[128] = {0};
            size_t remaining_len = pkt->payload_len - (page_req - payload_str) - 5;
            size_t copy_len = remaining_len < sizeof(path) - 1 ? remaining_len : sizeof(path) - 1;

            strncpy(path, page_req + 5, copy_len); // Extract path
            path[copy_len] = '\0';

            // Clean up trailing non-printable characters or msgpack framing
            for (int i = 0; i < copy_len; i++) {
                if (path[i] < ' ' || path[i] > '~') {
                    path[i] = '\0';
                    break;
                }
            }

            ESP_LOGI(TAG, "Parsed NomadNet Page Request: %s", path);

            // Pass the request to the application layer to generate a response
            // Use an MTU-safe buffer size for SX1262 LoRa transmission
            // Max payload size over SX1262 is 255 bytes, minus RNS header overhead
            char response_buf[230];
            size_t response_len = lxmf_nomadnet_generate_response(path, response_buf, sizeof(response_buf), pkt->recv_interface);

            if (response_len > 0) {
                // We have a payload to send back.
                ESP_LOGI(TAG, "Response generated, ready for TX.");

                // If this is part of an encrypted link session, we must encrypt the reply
                extern bool rns_link_send_encrypted_global(const uint8_t* data, size_t len, rns_interface_t ifac);

                // Try to send via active link first
                if (rns_link_send_encrypted_global((uint8_t*)response_buf, response_len, pkt->recv_interface)) {
                     ESP_LOGI(TAG, "Response sent securely over active link on interface %d", pkt->recv_interface);
                } else {
                    // Fallback to cleartext broadcast (for dev/debugging without link)
                    ESP_LOGI(TAG, "No active link, sending response in cleartext on interface %d", pkt->recv_interface);

                    rns_packet_t reply_pkt;
                    memset(&reply_pkt, 0, sizeof(reply_pkt));
                    // Reply to whoever sent this (echoing source to dest)
                    memcpy(reply_pkt.dest_hash, pkt->dest_hash, 16); // In a real implementation this would map source -> dest
                    reply_pkt.type = RNS_PKT_TYPE_DATA;
                    reply_pkt.flags = RNS_PKT_FLAG_HEADER; // minimal flags
                    reply_pkt.hops = 0;
                    reply_pkt.payload = (uint8_t*)response_buf;
                    reply_pkt.payload_len = response_len;

                    uint8_t out_buf[255];
                    size_t out_len = 0;
                    if (rns_encode_packet(&reply_pkt, out_buf, &out_len)) {
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
        }
    }

    return false;
}
