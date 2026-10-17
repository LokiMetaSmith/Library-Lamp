#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the Playnet client
void playnet_client_init(void);

// Send a JSON-RPC request to playnet.earth over the available transport (Wi-Fi or LoRa)
// For Wi-Fi, it uses esp_http_client directly.
// For LoRa, it encapsulates it in a Reticulum packet.
esp_err_t playnet_mcp_request(const char *method, const char *params_json, char **response_json_out);

// Push a bulletin board post to Playnet commitments
esp_err_t playnet_push_post(const char *post_json);

#ifdef __cplusplus
}
#endif
