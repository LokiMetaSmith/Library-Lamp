#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "playnet_client.h"
#include "bulletin_board.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "PLAYNET_SYNC";

static void playnet_sync_task(void *pvParameters) {
    ESP_LOGI(TAG, "Playnet Sync Task started.");

    while(1) {
        // Wait for 10 minutes between syncs
        vTaskDelay(pdMS_TO_TICKS(10 * 60 * 1000));

        ESP_LOGI(TAG, "Polling playnet for new community updates...");

        // Actually fetch and process the payload
        char *response = NULL;
        esp_err_t err = playnet_mcp_request("our-slots", "{}", &response);
        if (err == ESP_OK && response != NULL) {
            ESP_LOGI(TAG, "Successfully pulled community updates from Playnet.");

            cJSON *root = cJSON_Parse(response);
            if (root) {
                cJSON *result = cJSON_GetObjectItem(root, "result");
                if (result && cJSON_IsArray(result)) {
                    int num_slots = cJSON_GetArraySize(result);
                    for (int i = 0; i < num_slots; i++) {
                        cJSON *slot = cJSON_GetArrayItem(result, i);
                        cJSON *j_desc = cJSON_GetObjectItem(slot, "spec");
                        cJSON *j_auth = cJSON_GetObjectItem(slot, "owner");
                        cJSON *j_kind = cJSON_GetObjectItem(slot, "type");

                        char text[300] = {0};
                        char author[24] = "Playnet Node";
                        char type[15] = "Notice";

                        if (j_desc && cJSON_IsString(j_desc)) {
                            strncpy(text, j_desc->valuestring, sizeof(text)-1);
                        }
                        if (j_auth && cJSON_IsString(j_auth)) {
                            strncpy(author, j_auth->valuestring, sizeof(author)-1);
                        }
                        if (j_kind && cJSON_IsString(j_kind)) {
                            strncpy(type, j_kind->valuestring, sizeof(type)-1);
                        }

                        if (strlen(text) > 0) {
                            bb_add_message(author, type, text, 72);
                        }
                    }
                }
                cJSON_Delete(root);
            }
            free(response);
        } else {
            ESP_LOGW(TAG, "Failed to pull community updates from Playnet. Will retry later.");
        }
    }
}

void playnet_start_sync_task(void) {
    // Increased stack size from 4096 to 10240 for mbedTLS HTTPS overhead
    xTaskCreate(playnet_sync_task, "playnet_sync_task", 10240, NULL, 5, NULL);
}
