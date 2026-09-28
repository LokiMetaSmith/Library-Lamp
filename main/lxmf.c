#include "lxmf.h"
#include <string.h>
#include "esp_log.h"
#include "mbedtls/sha256.h"

static const char* TAG = "LXMF";

void lxmf_init(void) {
    ESP_LOGI(TAG, "Initializing Minimal LXMF Layer");
}

bool lxmf_handle_packet(const rns_packet_t* pkt) {
    if (!pkt || pkt->type != RNS_PKT_TYPE_DATA) {
        return false;
    }

    ESP_LOGI(TAG, "Received LXMF packet of length %d", pkt->payload_len);
    // Parse LXMF payload (source, destination, title, content)
    // Minimal mock implementation
    return true;
}
