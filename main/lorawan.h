#ifndef LORAWAN_H
#define LORAWAN_H

// Wio-SX1262 LoRa module pins (XIAO ESP32S3)
#define LORA_USE_SX1262

// SPI Interface
#ifdef CONFIG_LORA_MISO_PIN
#define LORA_MISO_PIN CONFIG_LORA_MISO_PIN
#else
#define LORA_MISO_PIN 8
#endif

#ifdef CONFIG_LORA_CLK_PIN
#define LORA_SCK_PIN  CONFIG_LORA_CLK_PIN
#else
#define LORA_SCK_PIN  7
#endif

#ifdef CONFIG_LORA_MOSI_PIN
#define LORA_MOSI_PIN CONFIG_LORA_MOSI_PIN
#else
#define LORA_MOSI_PIN 9
#endif

#ifdef CONFIG_LORA_CS_PIN
#define LORA_CS_PIN   CONFIG_LORA_CS_PIN
#else
#define LORA_CS_PIN   5
#endif

// Control Pins
#ifdef CONFIG_LORA_RST_PIN
#define LORA_RST_PIN  CONFIG_LORA_RST_PIN
#else
#define LORA_RST_PIN  3
#endif

#ifdef CONFIG_LORA_BUSY_PIN
#define LORA_BUSY_PIN CONFIG_LORA_BUSY_PIN
#else
#define LORA_BUSY_PIN 4
#endif

#ifdef CONFIG_LORA_DIO1_PIN
#define LORA_DIO1_PIN CONFIG_LORA_DIO1_PIN
#else
#define LORA_DIO1_PIN 2
#endif

#ifdef CONFIG_LORA_DIO2_PIN
#define LORA_DIO2_PIN CONFIG_LORA_DIO2_PIN
#else
#define LORA_DIO2_PIN 6 // DIO2 typically controls the RF switch
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern bool lora_scanning;

void lora_wan_init(void);
void lora_wan_broadcast(const char *message);

#ifdef __cplusplus
}
#endif

#endif // LORAWAN_H
