#include "reticulum.h"
#include <string.h>
#include "esp_log.h"
#include "mbedtls/sha256.h"
#include "mbedtls/aes.h"

static const char* TAG = "RETICULUM";

void rns_init(void) {
    ESP_LOGI(TAG, "Initializing Minimal Reticulum Framing Layer");
}

bool rns_encode_packet(rns_packet_t* pkt, uint8_t* out_buffer, size_t* out_len) {
    if (!pkt || !out_buffer || !out_len) return false;

    // Header flags: [ IFAC (1) | HEADER (1) | MAC (1) | PROPAGATE (1) | TYPE (2) | HOPS (2) ]
    uint8_t header_byte = 0;
    header_byte |= (pkt->flags & (RNS_PKT_FLAG_IFAC | RNS_PKT_FLAG_HEADER | RNS_PKT_FLAG_MAC | RNS_PKT_FLAG_PROPAGATE));
    header_byte |= (pkt->type & 0x03) << 2;
    header_byte |= (pkt->hops & 0x03);

    size_t offset = 0;
    out_buffer[offset++] = header_byte;

    // Simplistic framing: Header byte + dest hash + payload
    memcpy(out_buffer + offset, pkt->dest_hash, 16);
    offset += 16;

    if (pkt->payload && pkt->payload_len > 0) {
        memcpy(out_buffer + offset, pkt->payload, pkt->payload_len);
        offset += pkt->payload_len;
    }

    *out_len = offset;
    return true;
}

bool rns_decode_packet(const uint8_t* buffer, size_t len, rns_packet_t* out_pkt) {
    if (!buffer || len < 17 || !out_pkt) return false;

    uint8_t header_byte = buffer[0];
    out_pkt->flags = header_byte & 0xF0;
    out_pkt->type = (rns_packet_type_t)((header_byte >> 2) & 0x03);
    out_pkt->hops = header_byte & 0x03;

    memcpy(out_pkt->dest_hash, buffer + 1, 16);

    size_t payload_size = len - 17;
    if (payload_size > 0) {
        out_pkt->payload = (uint8_t*)(buffer + 17); // Point directly to buffer payload
        out_pkt->payload_len = payload_size;
    } else {
        out_pkt->payload = NULL;
        out_pkt->payload_len = 0;
    }

    return true;
}
