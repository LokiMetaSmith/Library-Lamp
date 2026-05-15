#ifndef ESP_HAL_H
#define ESP_HAL_H

#include <RadioLib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "hal/gpio_hal.h"

// define Arduino-style macros for compatibility with RadioLib
#define LOW                         (0x0)
#define HIGH                        (0x1)
#define INPUT                       (0x01)
#define OUTPUT                      (0x03)
#define RISING                      (0x01)
#define FALLING                     (0x02)

class EspHal : public RadioLibHal {
  public:
    EspHal(int8_t sck, int8_t miso, int8_t mosi)
      : RadioLibHal(INPUT, OUTPUT, LOW, HIGH, RISING, FALLING),
      spiSCK(sck), spiMISO(miso), spiMOSI(mosi), spiHandle(nullptr) {
    }

    void init() override {
        spiBegin();
    }

    void term() override {
        spiEnd();
    }

    void pinMode(uint32_t pin, uint32_t mode) override {
        if(pin == RADIOLIB_NC) {
            return;
        }

        gpio_config_t conf = {
            .pin_bit_mask = (1ULL << pin),
            .mode = (mode == INPUT) ? GPIO_MODE_INPUT : GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&conf);
    }

    void digitalWrite(uint32_t pin, uint32_t value) override {
        if(pin == RADIOLIB_NC) {
            return;
        }
        gpio_set_level((gpio_num_t)pin, value);
    }

    uint32_t digitalRead(uint32_t pin) override {
        if(pin == RADIOLIB_NC) {
            return(0);
        }
        return(gpio_get_level((gpio_num_t)pin));
    }

    void attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) override {
        if(interruptNum == RADIOLIB_NC) {
            return;
        }

        gpio_int_type_t intr_type = GPIO_INTR_DISABLE;
        if (mode == RISING) intr_type = GPIO_INTR_POSEDGE;
        else if (mode == FALLING) intr_type = GPIO_INTR_NEGEDGE;
        else intr_type = GPIO_INTR_ANYEDGE;

        gpio_set_intr_type((gpio_num_t)interruptNum, intr_type);
        gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
        gpio_isr_handler_add((gpio_num_t)interruptNum, (gpio_isr_t)interruptCb, NULL);
    }

    void detachInterrupt(uint32_t interruptNum) override {
        if(interruptNum == RADIOLIB_NC) {
            return;
        }
        gpio_isr_handler_remove((gpio_num_t)interruptNum);
        gpio_set_intr_type((gpio_num_t)interruptNum, GPIO_INTR_DISABLE);
    }

    void delay(unsigned long ms) override {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }

    void delayMicroseconds(unsigned long us) override {
        uint64_t start = esp_timer_get_time();
        while ((esp_timer_get_time() - start) < us) {
            // spin wait
        }
    }

    unsigned long millis() override {
        return (unsigned long)(esp_timer_get_time() / 1000ULL);
    }

    unsigned long micros() override {
        return (unsigned long)(esp_timer_get_time());
    }

    long pulseIn(uint32_t pin, uint32_t state, unsigned long timeout) override {
        if(pin == RADIOLIB_NC) {
            return(0);
        }

        this->pinMode(pin, INPUT);
        uint32_t start = this->micros();
        uint32_t curtick = this->micros();

        while(this->digitalRead(pin) == state) {
            if((this->micros() - curtick) > timeout) {
                return(0);
            }
        }

        return(this->micros() - start);
    }

    void spiBegin() {
        spi_bus_config_t buscfg;
        memset(&buscfg, 0, sizeof(spi_bus_config_t));
        buscfg.mosi_io_num = spiMOSI;
        buscfg.miso_io_num = spiMISO;
        buscfg.sclk_io_num = spiSCK;
        buscfg.quadwp_io_num = -1;
        buscfg.quadhd_io_num = -1;
        buscfg.data4_io_num = -1;
        buscfg.data5_io_num = -1;
        buscfg.data6_io_num = -1;
        buscfg.data7_io_num = -1;
        buscfg.max_transfer_sz = 256;

        // Initialize the SPI bus on SPI2_HOST (FSPI)
        esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK) {
            ESP_LOGE("ESPHAL", "Failed to initialize SPI bus.");
            return;
        }

        spi_device_interface_config_t devcfg;
        memset(&devcfg, 0, sizeof(spi_device_interface_config_t));
        devcfg.mode = 0;                                // SPI mode 0
        devcfg.clock_speed_hz = 2 * 1000 * 1000;        // Clock out at 2 MHz
        devcfg.spics_io_num = -1;                       // CS is handled manually by RadioLib
        devcfg.queue_size = 7;

        // Attach the Device to the SPI bus
        ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spiHandle);
        if (ret != ESP_OK) {
            ESP_LOGE("ESPHAL", "Failed to add SPI device.");
        }
    }

    void spiBeginTransaction() {}

    uint8_t spiTransferByte(uint8_t b) {
        uint8_t rx_data = 0;
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = 8;                     // length is in bits
        t.tx_buffer = &b;
        t.rx_buffer = &rx_data;

        spi_device_polling_transmit(spiHandle, &t);
        return rx_data;
    }

    void spiTransfer(uint8_t* out, size_t len, uint8_t* in) {
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = len * 8;               // length is in bits
        t.tx_buffer = out;
        t.rx_buffer = in;

        spi_device_polling_transmit(spiHandle, &t);
    }

    void spiEndTransaction() {}

    void spiEnd() {
        if (spiHandle != nullptr) {
            spi_bus_remove_device(spiHandle);
            spiHandle = nullptr;
        }
        spi_bus_free(SPI2_HOST);
    }

  private:
    int8_t spiSCK;
    int8_t spiMISO;
    int8_t spiMOSI;
    spi_device_handle_t spiHandle;
};

#endif
