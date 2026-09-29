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

#include "sodium.h"

rns_identity_t g_rns_local_identity;

void rns_identity_init(void) {
    if (sodium_init() < 0) {
        ESP_LOGE(TAG, "libsodium initialization failed!");
        return;
    }

    // In a real device, these keys should be persisted to NVS or SPIFFS so identity is retained.
    // For this minimal implementation, we generate an ephemeral identity per boot.
    crypto_sign_ed25519_keypair(g_rns_local_identity.public_key, g_rns_local_identity.private_key);

    // Hash the public key to get the identity hash (using sha256 as per Reticulum identity generation)
    mbedtls_sha256(g_rns_local_identity.public_key, ED25519_PUBLIC_KEY_BYTES, g_rns_local_identity.hash, 0); // 0 = SHA-256

    char hash_str[33] = {0};
    for (int i = 0; i < 16; i++) {
        sprintf(&hash_str[i * 2], "%02X", g_rns_local_identity.hash[i]);
    }
    ESP_LOGI(TAG, "Generated Ephemeral Reticulum Identity: %s", hash_str);
}

void rns_generate_announce_packet(rns_packet_t* pkt_out, uint8_t* payload_buf, const char* app_data) {
    if (!pkt_out || !payload_buf) return;

    memset(pkt_out, 0, sizeof(rns_packet_t));

    // Setup announce header
    pkt_out->type = RNS_PKT_TYPE_ANNOUNCE;
    pkt_out->flags = RNS_PKT_FLAG_HEADER; // Need header flag set
    pkt_out->hops = 0;

    // The destination hash for an announce is derived from the public key, application data, and a random token.
    // For this minimal stub, we will just use the identity hash as the announce destination (simplified).
    memcpy(pkt_out->dest_hash, g_rns_local_identity.hash, 16);

    // Build the Announce payload: [ Public Key (32) | App Data | Signature (64) ]
    size_t app_data_len = app_data ? strlen(app_data) : 0;

    memcpy(payload_buf, g_rns_local_identity.public_key, ED25519_PUBLIC_KEY_BYTES);
    if (app_data_len > 0) {
        memcpy(payload_buf + ED25519_PUBLIC_KEY_BYTES, app_data, app_data_len);
    }

    // Sign the payload (key + app_data)
    uint8_t* sig_ptr = payload_buf + ED25519_PUBLIC_KEY_BYTES + app_data_len;
    unsigned long long sig_len;
    crypto_sign_ed25519_detached(sig_ptr, &sig_len, payload_buf, ED25519_PUBLIC_KEY_BYTES + app_data_len, g_rns_local_identity.private_key);

    pkt_out->payload = payload_buf;
    pkt_out->payload_len = ED25519_PUBLIC_KEY_BYTES + app_data_len + 64; // Ed25519 sig is 64 bytes
}

bool rns_verify_announce_packet(const rns_packet_t* pkt) {
    if (!pkt || pkt->type != RNS_PKT_TYPE_ANNOUNCE || pkt->payload_len < (ED25519_PUBLIC_KEY_BYTES + 64)) {
        return false;
    }

    const uint8_t* pub_key = pkt->payload;
    size_t signed_data_len = pkt->payload_len - 64;
    const uint8_t* sig = pkt->payload + signed_data_len;

    if (crypto_sign_ed25519_verify_detached(sig, pkt->payload, signed_data_len, pub_key) == 0) {
        ESP_LOGI(TAG, "Announce packet signature verified successfully!");
        return true;
    }

    ESP_LOGW(TAG, "Announce packet signature verification failed.");
    return false;
}
