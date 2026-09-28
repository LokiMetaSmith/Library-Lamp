#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "reticulum.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t source_hash[16];
    uint8_t dest_hash[16];
    const char* title;
    const uint8_t* content;
    size_t content_len;
} lxmf_message_t;

void lxmf_init(void);
bool lxmf_handle_packet(const rns_packet_t* pkt);

#ifdef __cplusplus
}
#endif
