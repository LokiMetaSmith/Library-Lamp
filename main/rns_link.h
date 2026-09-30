#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "reticulum.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LINK_STATE_CLOSED,
    LINK_STATE_HANDSHAKING,
    LINK_STATE_ACTIVE,
    LINK_STATE_TEARDOWN
} link_state_t;

typedef struct {
    uint8_t link_id[16];
    uint8_t peer_pubkey[ED25519_PUBLIC_KEY_BYTES];

    // X25519 Ephemeral Keypair for this link
    uint8_t ecdh_pubkey[32];
    uint8_t ecdh_privkey[32];

    uint8_t shared_key[32]; // ChaCha20/AES key derived from X25519
    link_state_t state;

    // File streaming state
    bool is_streaming;
    const char* streaming_filepath;
    void* stream_fp; // FILE* pointer
    size_t streaming_offset;
    uint32_t last_activity_ms;

    // ARQ
    uint16_t next_seq;
    uint16_t unacked_seq;

    // Interface
    rns_interface_t ifac;
} rns_link_t;

void rns_link_init(void);
bool rns_link_process_packet(const rns_packet_t* pkt);
void rns_link_start_file_stream(rns_link_t* link, const char* filepath);
void rns_link_tick(void); // Call from main loop to process outgoing file chunks

#ifdef __cplusplus
}
#endif
