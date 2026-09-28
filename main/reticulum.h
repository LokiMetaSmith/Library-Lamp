#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RNS_HEADER_MAX_LEN 16
#define RNS_MAC_LEN 16
#define RNS_IFAC_MIN_LEN 2

#define RNS_PKT_FLAG_IFAC 0x80
#define RNS_PKT_FLAG_HEADER 0x40
#define RNS_PKT_FLAG_MAC 0x20
#define RNS_PKT_FLAG_PROPAGATE 0x10

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
} rns_packet_t;

void rns_init(void);
bool rns_encode_packet(rns_packet_t* pkt, uint8_t* out_buffer, size_t* out_len);
bool rns_decode_packet(const uint8_t* buffer, size_t len, rns_packet_t* out_pkt);

#ifdef __cplusplus
}
#endif
