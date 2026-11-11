#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#define RNS_HEADER_MAX_LEN 16
#define RNS_MAC_LEN 16
#define RNS_IFAC_MIN_LEN 2

#define RNS_PKT_FLAG_IFAC 0x80
#define RNS_PKT_FLAG_HEADER 0x40
#define RNS_PKT_FLAG_MAC 0x20
#define RNS_PKT_FLAG_PROPAGATE 0x10

typedef enum {
    IF_NONE = 0,
    IF_LORA = 1,
    IF_WIFI = 2,
    IF_ANY = 3
} rns_interface_t;

typedef enum {
    RNS_PKT_TYPE_DATA = 0x00,
    RNS_PKT_TYPE_ANNOUNCE = 0x01,
    RNS_PKT_TYPE_LINKREQUEST = 0x02,
    RNS_PKT_TYPE_PROOF = 0x03
} rns_packet_type_t;

typedef struct {
    uint8_t dest_hash[16];
    uint8_t flags;
    rns_packet_type_t type;
    uint8_t hops;
    uint8_t* payload;
    size_t payload_len;
    rns_interface_t recv_interface;
} rns_packet_t;

#define ESP_LOGE(tag, fmt, ...) printf("ERROR [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) printf("INFO [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("WARN [%s] " fmt "\n", tag, ##__VA_ARGS__)

static const char* TAG = "RETICULUM_TEST";

// Under test
bool rns_encode_packet(rns_packet_t* pkt, uint8_t* out_buffer, size_t max_len, size_t* out_len) {
    if (!pkt || !out_buffer || !out_len) return false;

    uint8_t header_byte = 0;
    header_byte |= (pkt->flags & (RNS_PKT_FLAG_IFAC | RNS_PKT_FLAG_HEADER | RNS_PKT_FLAG_MAC | RNS_PKT_FLAG_PROPAGATE));
    header_byte |= (pkt->type & 0x03) << 2;
    header_byte |= (pkt->hops & 0x03);

    size_t payload_len = (pkt->payload != NULL) ? pkt->payload_len : 0;
    size_t required_len = 1 + 16 + payload_len;
    if (required_len > max_len || required_len > 255) {
        ESP_LOGE(TAG, "Failed to encode RNS packet: Required length %zu exceeds limit (max_len=%zu, MTU=255)", required_len, max_len);
        return false;
    }

    size_t offset = 0;
    out_buffer[offset++] = header_byte;

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
    if (!buffer || len < 17 || len > 255 || !out_pkt) return false;

    memset(out_pkt, 0, sizeof(rns_packet_t));

    uint8_t header_byte = buffer[0];
    out_pkt->flags = header_byte & 0xF0;
    out_pkt->type = (rns_packet_type_t)((header_byte >> 2) & 0x03);
    out_pkt->hops = header_byte & 0x03;

    memcpy(out_pkt->dest_hash, buffer + 1, 16);

    size_t payload_size = len - 17;
    if (payload_size > 0) {
        out_pkt->payload = (uint8_t*)(buffer + 17);
        out_pkt->payload_len = payload_size;
    } else {
        out_pkt->payload = NULL;
        out_pkt->payload_len = 0;
    }

    return true;
}

void test_encode_decode_roundtrip() {
    rns_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    for (int i = 0; i < 16; i++) pkt.dest_hash[i] = (uint8_t)(0x10 + i);
    pkt.flags = RNS_PKT_FLAG_HEADER | RNS_PKT_FLAG_PROPAGATE;
    pkt.type = RNS_PKT_TYPE_DATA;
    pkt.hops = 2;

    const char* sample_payload = "Hello Reticulum Mesh!";
    pkt.payload = (uint8_t*)sample_payload;
    pkt.payload_len = strlen(sample_payload);

    uint8_t buffer[255];
    size_t encoded_len = 0;

    bool enc_res = rns_encode_packet(&pkt, buffer, sizeof(buffer), &encoded_len);
    assert(enc_res == true);
    assert(encoded_len == 1 + 16 + strlen(sample_payload));

    rns_packet_t decoded;
    bool dec_res = rns_decode_packet(buffer, encoded_len, &decoded);
    assert(dec_res == true);
    assert(decoded.flags == (RNS_PKT_FLAG_HEADER | RNS_PKT_FLAG_PROPAGATE));
    assert(decoded.type == RNS_PKT_TYPE_DATA);
    assert(decoded.hops == 2);
    assert(memcmp(decoded.dest_hash, pkt.dest_hash, 16) == 0);
    assert(decoded.payload_len == strlen(sample_payload));
    assert(memcmp(decoded.payload, sample_payload, decoded.payload_len) == 0);

    printf("test_encode_decode_roundtrip passed.\n");
}

void test_empty_payload() {
    rns_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    for (int i = 0; i < 16; i++) pkt.dest_hash[i] = 0xAA;
    pkt.flags = RNS_PKT_FLAG_HEADER;
    pkt.type = RNS_PKT_TYPE_PROOF;
    pkt.hops = 0;
    pkt.payload = NULL;
    pkt.payload_len = 0;

    uint8_t buffer[255];
    size_t encoded_len = 0;

    bool enc_res = rns_encode_packet(&pkt, buffer, sizeof(buffer), &encoded_len);
    assert(enc_res == true);
    assert(encoded_len == 17);

    rns_packet_t decoded;
    bool dec_res = rns_decode_packet(buffer, encoded_len, &decoded);
    assert(dec_res == true);
    assert(decoded.payload == NULL);
    assert(decoded.payload_len == 0);
    assert(decoded.type == RNS_PKT_TYPE_PROOF);

    printf("test_empty_payload passed.\n");
}

void test_mtu_limit_and_bounds() {
    rns_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    uint8_t large_payload[300];
    memset(large_payload, 0x42, sizeof(large_payload));
    pkt.payload = large_payload;
    pkt.payload_len = 250; // 1 + 16 + 250 = 267 > 255 (MTU limit)

    uint8_t buffer[300];
    size_t encoded_len = 0;

    // Should fail because 267 > 255 MTU
    bool enc_res = rns_encode_packet(&pkt, buffer, sizeof(buffer), &encoded_len);
    assert(enc_res == false);

    // Should fail with small buffer
    pkt.payload_len = 10;
    enc_res = rns_encode_packet(&pkt, buffer, 10, &encoded_len);
    assert(enc_res == false);

    // Truncated buffer decoding (< 17 bytes)
    uint8_t short_buf[16];
    rns_packet_t decoded;
    bool dec_res = rns_decode_packet(short_buf, sizeof(short_buf), &decoded);
    assert(dec_res == false);

    // Buffer exceeding MTU
    dec_res = rns_decode_packet(large_payload, 256, &decoded);
    assert(dec_res == false);

    // NULL pointers
    assert(rns_encode_packet(NULL, buffer, 255, &encoded_len) == false);
    assert(rns_encode_packet(&pkt, NULL, 255, &encoded_len) == false);
    assert(rns_encode_packet(&pkt, buffer, 255, NULL) == false);
    assert(rns_decode_packet(NULL, 17, &decoded) == false);
    assert(rns_decode_packet(buffer, 17, NULL) == false);

    printf("test_mtu_limit_and_bounds passed.\n");
}

int main() {
    test_encode_decode_roundtrip();
    test_empty_payload();
    test_mtu_limit_and_bounds();
    printf("All reticulum unit tests passed!\n");
    return 0;
}
