#include "lorawan.h"
#include "esp_log.h"

static const char *TAG = "LORAWAN";

void lora_wan_init(void) {
    ESP_LOGI(TAG, "LoRaWAN initializing (stub)");
}

void lora_wan_broadcast(const char *message) {
    ESP_LOGI(TAG, "Broadcasting via LoRaWAN: %s", message);
}
