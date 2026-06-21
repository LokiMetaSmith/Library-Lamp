#include "lorawan.h"
#include "esp_log.h"
#include <RadioLib.h>
#include "EspHal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "freertos/queue.h"

// Queue for async transmissions
QueueHandle_t loraTransmitQueue = NULL;
bool lora_scanning = false;
#define LORA_HEARTBEAT_INTERVAL_MS 60000 // 1 minute


#include "freertos/queue.h"

// Queue for async transmissions
QueueHandle_t loraTransmitQueue = NULL;
bool lora_scanning = false;
#define LORA_HEARTBEAT_INTERVAL_MS 60000 // 1 minute


static const char *TAG = "LORAWAN";

// Global pointers for the HAL and Radio instance
EspHal* hal = nullptr;
SX1262* radio = nullptr;

// FreeRTOS task handle and mutex
bool lora_initialized = false;
TaskHandle_t loraReceiveTaskHandle = NULL;
SemaphoreHandle_t loraMutex = NULL;

void loraReceiveTask(void *pvParameters) {
    ESP_LOGI(TAG, "Starting LoRa receive loop...");

    // Switch to receive mode
    int state = radio->startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to start receive, code %d", state);
    }

    while (1) {
        // Poll DIO1 pin for IRQ events (like RX Done)
        if (hal->digitalRead(LORA_DIO1_PIN) == HIGH) {
            if (xSemaphoreTake(loraMutex, portMAX_DELAY) == pdTRUE) {
                uint8_t str[256];
                memset(str, 0, 256);
                state = radio->readData(str, 255);

                if (state == RADIOLIB_ERR_NONE) {
                    ESP_LOGI(TAG, "Received packet!");
                    ESP_LOGI(TAG, "Data: %s", (char*)str);
                    ESP_LOGI(TAG, "RSSI: %f dBm", radio->getRSSI());
                    ESP_LOGI(TAG, "SNR: %f dB", radio->getSNR());

                    // TODO: Parse MSG packets and inject them into bulletin_board
                } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
                    ESP_LOGW(TAG, "CRC error!");
                } else {
                    ESP_LOGW(TAG, "Failed to read data, code %d", state);
                }
                // Clear IRQ flags and restart receive mode
                // IRQ flags cleared automatically by RadioLib startReceive
                radio->startReceive();

                xSemaphoreGive(loraMutex);
            }
        }

        // Yield to RTOS scheduler
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}



void loraTransmitTask(void *pvParameters) {
    ESP_LOGI(TAG, "Starting LoRa transmit/heartbeat task...");
    char msg[128];
    TickType_t lastHeartbeat = xTaskGetTickCount();

    while (1) {
        // Wait for a message to transmit, or timeout after heartbeat interval
        if (xQueueReceive(loraTransmitQueue, msg, pdMS_TO_TICKS(1000)) == pdPASS) {
            ESP_LOGI(TAG, "Async broadcasting via LoRaWAN: %s", msg);
            if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                lora_scanning = true;
                int state = radio->transmit(msg);
                if (state == RADIOLIB_ERR_NONE) {
                    ESP_LOGI(TAG, "Broadcast success!");
                } else {
                    ESP_LOGE(TAG, "Broadcast failed, code %d", state);
                }
                lora_scanning = false;
                radio->startReceive();
                xSemaphoreGive(loraMutex);
            } else {
                ESP_LOGE(TAG, "Failed to acquire LoRa mutex for async broadcast");
            }
        }

        // Periodic Heartbeat
        if ((xTaskGetTickCount() - lastHeartbeat) > pdMS_TO_TICKS(LORA_HEARTBEAT_INTERVAL_MS)) {
            if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                lora_scanning = true;
                const char* heartbeat_msg = "HEARTBEAT: Library-Lamp Discovery";
                ESP_LOGI(TAG, "Sending Discovery Heartbeat");
                radio->transmit(heartbeat_msg);
                lora_scanning = false;
                radio->startReceive();
                xSemaphoreGive(loraMutex);
            }
            lastHeartbeat = xTaskGetTickCount();
        }
    }
}

void lora_wan_init(void) {
    ESP_LOGI(TAG, "LoRaWAN initializing (SX1262 / RadioLib)");

    // Instantiate our custom ESP-IDF SPI HAL
    hal = new EspHal(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN);

    // Create the mutex for thread safety
    loraMutex = xSemaphoreCreateMutex();

    // Create transmission queue
    loraTransmitQueue = xQueueCreate(5, 128);



    // Create the generic RadioLib module wrapper
    Module* mod = new Module(hal, LORA_CS_PIN, LORA_DIO1_PIN, LORA_RST_PIN, LORA_BUSY_PIN);

    // Instantiate the SX1262 module using the HAL
    radio = new SX1262(mod);

    // Initialize radio. LongFast: 906.875 MHz? Meshtastic defaults to 915 MHz in US
    // We'll use 915.0 MHz base, but the specific channel doesn't matter too much for P2P testing
    int state = radio->begin(915.0);

    if (state == RADIOLIB_ERR_NONE) {
        ESP_LOGI(TAG, "SX1262 init success!"); lora_initialized = true;

        // Apply Meshtastic LongFast equivalents
        radio->setBandwidth(250.0);
        radio->setSpreadingFactor(11);
        radio->setCodingRate(8);     // CR = 4/8 -> 8 in RadioLib API
        radio->setSyncWord(0x2B);    // Meshtastic standard sync word
        radio->setOutputPower(22);   // Max power for SX1262 usually

        // Configure DIO2 as RF switch (standard for many SX1262 modules)
        radio->setTCXO(1.8);         // Typical TCXO voltage on these modules
        radio->setDio2AsRfSwitch(true);

        // Start receive task
        xTaskCreatePinnedToCore(
            loraReceiveTask,
            "LoRaReceive",
            4096,
            NULL,
            5,
            &loraReceiveTaskHandle,
            1
        );

    } else {
        ESP_LOGW(TAG, "SX1262 init failed (soft fail), code %d", state);
        lora_initialized = false;
    }
}

void lora_wan_broadcast(const char *message) {
    if (!lora_initialized || loraTransmitQueue == NULL) {
        ESP_LOGE(TAG, "Radio not initialized or queue not created!");
        return;
    }
    ESP_LOGI(TAG, "Enqueueing LoRaWAN broadcast: %s", message);
    if (xQueueSend(loraTransmitQueue, message, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "Failed to enqueue LoRaWAN broadcast (queue full)");
    }
}
