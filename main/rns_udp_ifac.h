#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void rns_udp_ifac_init(void);
void rns_udp_transmit_raw(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif
