#ifndef LORAWAN_H
#define LORAWAN_H

// Wio-SX1262 LoRa module pins (XIAO ESP32S3)
#define LORA_USE_SX1262

// SPI Interface
#define LORA_MISO_PIN 8
#define LORA_SCK_PIN  7
#define LORA_MOSI_PIN 9
#define LORA_CS_PIN   5

// Control Pins
#define LORA_RST_PIN  3
#define LORA_BUSY_PIN 4
#define LORA_DIO1_PIN 2
#define LORA_DIO2_PIN 6 // DIO2 typically controls the RF switch

#ifdef __cplusplus
extern "C" {
#endif

void lora_wan_init(void);
void lora_wan_broadcast(const char *message);

#ifdef __cplusplus
}
#endif

#endif // LORAWAN_H
