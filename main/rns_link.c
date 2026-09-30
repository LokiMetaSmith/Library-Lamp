#include "rns_link.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sodium.h"
#include "lxmf.h"

static const char* TAG = "RNS_LINK";

// Interface-aware link tracking (1 for LoRa, 1 for Wi-Fi)
static rns_link_t g_lora_link = {0};
static rns_link_t g_wifi_link = {0};

static rns_link_t* get_link_for_ifac(rns_interface_t ifac) {
    if (ifac == IF_WIFI) return &g_wifi_link;
    return &g_lora_link;
}

void rns_link_init(void) {
    ESP_LOGI(TAG, "Initializing Minimal Reticulum Link Layer (Dual Interface)");
    memset(&g_lora_link, 0, sizeof(g_lora_link));
    g_lora_link.state = LINK_STATE_CLOSED;
    g_lora_link.ifac = IF_LORA;

    memset(&g_wifi_link, 0, sizeof(g_wifi_link));
    g_wifi_link.state = LINK_STATE_CLOSED;
    g_wifi_link.ifac = IF_WIFI;
}

// Network transmit for ARQ chunks
extern void lora_wan_transmit_raw(const uint8_t *data, size_t len);
extern void rns_udp_transmit_raw(const uint8_t *data, size_t len);

static void send_link_packet(rns_link_t* link, const uint8_t* payload, size_t len, const uint8_t* dest_hash) {
    if (!link) return;

    rns_packet_t reply = {0};
    reply.flags = RNS_PKT_FLAG_HEADER; // Minimal
    reply.type = RNS_PKT_TYPE_DATA;
    reply.hops = 0;

    // Simplistic routing - reply to the link ID
    if (dest_hash) memcpy(reply.dest_hash, dest_hash, 16);
    else memcpy(reply.dest_hash, link->link_id, 16);
    reply.payload = (uint8_t*)payload;
    reply.payload_len = len;

    uint8_t out_buf[255];
    size_t out_len = 0;
    if (rns_encode_packet(&reply, out_buf, &out_len)) {
        if (link->ifac == IF_WIFI) {
            rns_udp_transmit_raw(out_buf, out_len);
        } else {
            lora_wan_transmit_raw(out_buf, out_len);
        }
    }
}

bool rns_link_process_packet(const rns_packet_t* pkt) {
    if (!pkt) return false;

    rns_link_t* link = get_link_for_ifac(pkt->recv_interface);

    if (pkt->type == RNS_PKT_TYPE_LINKREQUEST) {
        // Prevent interrupting an active link on this interface
        if (link->state != LINK_STATE_CLOSED) {
            ESP_LOGW(TAG, "LinkRequest rejected: Interface %d is already busy with an active link", pkt->recv_interface);
            return false;
        }

        ESP_LOGI(TAG, "Received LinkRequest on interface %d, transitioning to HANDSHAKING", pkt->recv_interface);

        // Ensure valid payload for X25519 public key (32 bytes)
        if (pkt->payload_len >= 32) {
            memcpy(link->peer_pubkey, pkt->payload, 32);

            // The destination hash of the link request is OUR identity.
            // The sender's identity is not guaranteed to be in the headers here in this minimal implementation,
            // so we will naively assume the link peer is the only one we are talking to,
            // and use a broadcast/derived link ID for this mock.
            // In full RNS, Link ID is SHA256 of the exchange.
            memcpy(link->link_id, link->peer_pubkey, 16); // Stub Link ID

            // Generate ephemeral X25519 keypair for handshake
            crypto_box_keypair(link->ecdh_pubkey, link->ecdh_privkey);

            // Derive shared secret
            if (crypto_scalarmult(link->shared_key, link->ecdh_privkey, link->peer_pubkey) != 0) {
                ESP_LOGE(TAG, "Failed to derive X25519 shared secret");
                return false;
            }

            ESP_LOGI(TAG, "Successfully derived X25519 shared secret");

            // Send LinkProof (Contains our ephemeral public key)
            // Using same simulated logic as send_link_packet but for PROOF type
            rns_packet_t proof_pkt = {0};
            proof_pkt.flags = RNS_PKT_FLAG_HEADER;
            proof_pkt.type = RNS_PKT_TYPE_PROOF;
            proof_pkt.hops = 0;
            memcpy(proof_pkt.dest_hash, pkt->dest_hash, 16); // Routing back
            proof_pkt.payload = link->ecdh_pubkey;
            proof_pkt.payload_len = 32;

            uint8_t out_buf[255];
            size_t out_len = 0;
            if (rns_encode_packet(&proof_pkt, out_buf, &out_len)) {
                if (link->ifac == IF_WIFI) {
                    rns_udp_transmit_raw(out_buf, out_len);
                } else {
                    lora_wan_transmit_raw(out_buf, out_len);
                }
            }

            link->state = LINK_STATE_ACTIVE;
            link->last_activity_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
            return true;
        }
        return false;
    }

    // Process incoming Link packets (acks, teardowns) only if the destination matches the link
    if (link->state == LINK_STATE_ACTIVE && pkt->type == RNS_PKT_TYPE_DATA) {
        if (memcmp(pkt->dest_hash, link->link_id, 16) == 0 || memcmp(pkt->dest_hash, g_rns_local_identity.hash, 16) == 0) {
            link->last_activity_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

            // Decrypt link packet using crypto_secretbox
            // Check if there is enough payload for MAC (16 bytes) + 3 bytes minimal payload
            if (pkt->payload_len >= (crypto_secretbox_MACBYTES + 3)) {
                // In a full implementation, we'd extract the nonce from the packet or use a synchronized counter.
                // For this implementation, we use a simple counter based on sequence number as nonce.
                // To do this properly, the packet needs to contain the sequence number unencrypted.
                // Since our minimal ARQ just sends ACKs, we'll try to decrypt it.
                // This requires a shared agreement on nonce handling.
                // Assuming simple static nonce for ACKs in this minimal stub for now.
                uint8_t nonce[crypto_secretbox_NONCEBYTES] = {0}; // Static nonce for ACKs for now

                uint8_t* decrypted_buf = malloc(pkt->payload_len - crypto_secretbox_MACBYTES);
                if (decrypted_buf) {
                    if (crypto_secretbox_open_easy(decrypted_buf, pkt->payload, pkt->payload_len, nonce, link->shared_key) == 0) {
                        // Successfully decrypted
                        // Simulated ACK parsing (naive)
                        if (decrypted_buf[0] == 0xAC) {
                            uint16_t ack_seq = (decrypted_buf[1] << 8) | decrypted_buf[2];
                            ESP_LOGI(TAG, "Received ACK for seq %d on interface %d", ack_seq, pkt->recv_interface);
                            link->unacked_seq = ack_seq + 1;
                        } else {
                            // If it's not an ACK, it might be LXMF data over the link. Pass it up.
                            // We need to create a dummy rns_packet_t with the decrypted payload
                            ESP_LOGI(TAG, "Passing decrypted link payload to LXMF");
                            rns_packet_t link_payload_pkt = *pkt; // copy headers
                            link_payload_pkt.payload = decrypted_buf;
                            link_payload_pkt.payload_len = pkt->payload_len - crypto_secretbox_MACBYTES;
                            lxmf_handle_packet(&link_payload_pkt);
                        }
                    } else {
                        ESP_LOGW(TAG, "Failed to decrypt link packet on interface %d", pkt->recv_interface);
                    }
                    free(decrypted_buf);
                }
            } else if (pkt->payload_len > 0) {
                 // Might be unencrypted during development/testing if peers aren't enforcing
                 // If it looks like a page request, try passing it to LXMF anyway as a fallback
                 const char* payload_str = (const char*)pkt->payload;
                 if (strnstr(payload_str, "page ", pkt->payload_len)) {
                     ESP_LOGW(TAG, "Received unencrypted page request on active link. Allowing for debug.");
                     lxmf_handle_packet(pkt);
                 }
            }

            return true; // We handled it, don't pass to LXMF routing loop
        }
    }

    return false;
}

// Send encrypted data over the active link
bool rns_link_send_encrypted(rns_link_t* link, const uint8_t* data, size_t len) {
    if (!link || link->state != LINK_STATE_ACTIVE) return false;

    // Very naive chunking for LXMF replies (assumes they fit in one chunk for this stub)
    // Add sequence number header
    uint8_t nonce[crypto_secretbox_NONCEBYTES] = {0};
    nonce[0] = (link->next_seq >> 8) & 0xFF;
    nonce[1] = link->next_seq & 0xFF;

    uint8_t* ciphertext_buf = malloc(len + 2 + crypto_secretbox_MACBYTES);
    if (!ciphertext_buf) return false;

    ciphertext_buf[0] = nonce[0]; // Seq High
    ciphertext_buf[1] = nonce[1]; // Low

    crypto_secretbox_easy(ciphertext_buf + 2, data, len, nonce, link->shared_key);

    send_link_packet(link, ciphertext_buf, len + 2 + crypto_secretbox_MACBYTES, link->link_id);
    link->next_seq++;

    free(ciphertext_buf);
    return true;
}

bool rns_link_send_encrypted_global(const uint8_t* data, size_t len, rns_interface_t ifac) {
    if (ifac == IF_WIFI && g_wifi_link.state == LINK_STATE_ACTIVE) {
        return rns_link_send_encrypted(&g_wifi_link, data, len);
    }
    if (ifac == IF_LORA && g_lora_link.state == LINK_STATE_ACTIVE) {
        return rns_link_send_encrypted(&g_lora_link, data, len);
    }
    return false;
}

void rns_link_start_file_stream(rns_link_t* link, const char* filepath) {
    if (!link || link->state != LINK_STATE_ACTIVE) {
        ESP_LOGW(TAG, "Cannot start file stream: link not active.");
        return;
    }

    if (link->is_streaming) {
        ESP_LOGW(TAG, "Already streaming on this link, ignoring new request");
        return;
    }

    ESP_LOGI(TAG, "Starting file stream for %s over link", filepath);

    FILE* f = fopen(filepath, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        return;
    }

    link->is_streaming = true;
    link->streaming_filepath = filepath;
    link->stream_fp = f;
    link->streaming_offset = 0;
    link->next_seq = 0;
    link->unacked_seq = 0;
}

// Helper so the NomadNet layer doesn't need to know about internal link arrays
void rns_link_start_file_stream_global(const char* filepath, rns_interface_t ifac) {
    if (ifac == IF_WIFI && g_wifi_link.state == LINK_STATE_ACTIVE && !g_wifi_link.is_streaming) {
        rns_link_start_file_stream(&g_wifi_link, filepath);
    } else if (ifac == IF_LORA && g_lora_link.state == LINK_STATE_ACTIVE && !g_lora_link.is_streaming) {
        rns_link_start_file_stream(&g_lora_link, filepath);
    } else {
        ESP_LOGW(TAG, "No active idle link found for streaming on requested interface.");
    }
}

static void tick_link(rns_link_t* link) {
    if (!link || link->state == LINK_STATE_CLOSED) return;

    if (link->state == LINK_STATE_ACTIVE && link->is_streaming) {
        // Wait for ACK before sending next chunk to avoid flooding LoRa (Stop-and-wait ARQ)
        if (link->next_seq > link->unacked_seq) {
             // Timeout handling for retransmission could go here
             return;
        }

        FILE* f = (FILE*)link->stream_fp;
        if (!f) {
             link->is_streaming = false;
             return;
        }

        // Buffer sizing for MTU compliance (255 bytes max for SX1262)
        // RNS Header (IFAC+HDR+MAC+PROPAGATE+TYPE+HOPS) = ~1 byte
        // Dest Hash = 16 bytes
        // MAC Auth Tag = 16 bytes
        // Sequence Number (Nonce) = 2 bytes
        // Total Overhead = ~35 bytes.
        // Therefore, maximum safe ciphertext chunk size is around 220 bytes.
        uint8_t chunk_buf[220]; // Keep under 255 MTU including headers
        size_t bytes_read = fread(chunk_buf + 2, 1, sizeof(chunk_buf) - 2, f);

        if (bytes_read > 0) {
            ESP_LOGI(TAG, "Tick: Streaming file chunk seq %d (%zu bytes) on ifac %d", link->next_seq, bytes_read, link->ifac);

            // Add sequence number header
            chunk_buf[0] = (link->next_seq >> 8) & 0xFF;
            chunk_buf[1] = link->next_seq & 0xFF;

            // Encrypt using shared key with libsodium crypto_secretbox
            // In a full implementation, we'd use a synchronized nonce or track it per packet.
            // For this implementation, we use a simple counter based on sequence number as nonce.
            uint8_t nonce[crypto_secretbox_NONCEBYTES] = {0};
            nonce[0] = (link->next_seq >> 8) & 0xFF;
            nonce[1] = link->next_seq & 0xFF;

            // chunk_buf has seq number at 0 and 1, followed by MAC (16 bytes) and ciphertext
            // To respect MTU, we need to ensure bytes_read + 2 + MAC fits in 230 bytes
            // MAC length for secretbox is crypto_secretbox_MACBYTES (16 bytes)

            uint8_t ciphertext_buf[255];
            ciphertext_buf[0] = chunk_buf[0]; // Seq High
            ciphertext_buf[1] = chunk_buf[1]; // Seq Low

            crypto_secretbox_easy(ciphertext_buf + 2, chunk_buf + 2, bytes_read, nonce, link->shared_key);

            send_link_packet(link, ciphertext_buf, bytes_read + 2 + crypto_secretbox_MACBYTES, link->link_id);
            link->next_seq++;

        } else {
            ESP_LOGI(TAG, "EOF reached. Stream finished on ifac %d.", link->ifac);
            fclose(f);
            link->stream_fp = NULL;
            link->is_streaming = false;
        }
    }

    // Timeout inactive links
    if (link->state != LINK_STATE_CLOSED) {
        if ((xTaskGetTickCount() * portTICK_PERIOD_MS) - link->last_activity_ms > 45000) {
            ESP_LOGI(TAG, "Link on ifac %d timed out. Tearing down.", link->ifac);
            link->state = LINK_STATE_CLOSED;
            if (link->is_streaming && link->stream_fp) {
                fclose((FILE*)link->stream_fp);
                link->stream_fp = NULL;
                link->is_streaming = false;
            }
        }
    }
}

void rns_link_tick(void) {
    tick_link(&g_lora_link);
    tick_link(&g_wifi_link);
}
