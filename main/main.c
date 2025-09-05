/*
 * =================================================================================
 * Project: E-Book Librarian (ESP-IDF Version)
 * Hardware: ESP32-S3 with a MicroSD card module (e.g., ESP32-S3-USB-OTG board)
 * Framework: ESP-IDF
 *
 * Description:
 * This project turns an ESP32-S3 into a "virtual bookshelf". This version is
 * built using the Espressif IoT Development Framework (ESP-IDF) to leverage
 * its robust USB Host stack.
 *
 * When an e-reader is connected, the ESP32-S3 mounts its storage via the USB
 * Host MSC (Mass Storage Class) driver. A web server provides an interface to
 * transfer files between the local SD card and the connected e-reader.
 *
 * This version also includes support for a WS2812/NeoPixel LED strip to
 * provide visual feedback on the device's status.
 *
 * NOTE: This file contains the core application logic. To compile this, you will
 * need to set up a standard ESP-IDF project, which includes a CMakeLists.txt
 * file and component configurations (e.g., via `idf.py menuconfig`).
 * =================================================================================
 */

// --- Standard and ESP-IDF Dependencies ---
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <math.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_vfs_fat.h"
#include "esp_spiffs.h"
#include "sdmmc_cmd.h"
#include "cJSON.h"

// --- USB Host Dependencies (from the official ESP-IDF stack) ---
#include "esp_usb.h"
#include "usb/usb_host.h"
#include "usb/msc_host.h"
#include "usb/vfs_msc.h"

// --- LED Strip Dependencies ---
#include "driver/rmt_tx.h"
#include "led_strip.h"


// --- CONFIGURATION ---
// Wi-Fi Access Point settings
#define WIFI_SSID      "Ebook-Library-Box"
#define WIFI_PASS      "sharebooks"
#define WIFI_MAX_STA_CONN 4

// SD Card mount point and pin configuration
#define MOUNT_POINT_SD "/sdcard"
#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5

// USB mount point
#define MOUNT_POINT_USB "/usb"

// SPIFFS mount point for web assets
#define MOUNT_POINT_SPIFFS "/spiffs"


// LED Strip configuration
#define LED_STRIP_GPIO              4
#define LED_STRIP_LED_NUMBERS       8
#define LED_STRIP_RMT_RES_HZ        (10 * 1000 * 1000) // 10MHz resolution

// --- GLOBALS ---
static const char *TAG = "EBOOK_LIBRARIAN";
static bool ebook_reader_connected = false;
static msc_host_device_handle_t device_handle = NULL;
static led_strip_handle_t g_led_strip;

// Enum and global variable for managing LED state
typedef enum {
    LED_STATE_INIT,
    LED_STATE_IDLE,
    LED_STATE_CONNECTED,
    LED_STATE_TRANSFER,
    LED_STATE_ERROR
} led_state_t;

volatile led_state_t g_led_state = LED_STATE_INIT;

// --- Transfer Progress Tracking ---
typedef struct {
    char filename[256];
    size_t bytes_transferred;
    size_t total_bytes;
    bool active;
    bool success;
    char error_msg[128];
} transfer_progress_t;

static transfer_progress_t g_transfer_progress = {
    .active = false,
};
volatile bool g_cancel_transfer = false;


// --- HELPER FUNCTIONS ---
// Helper to copy file between two filesystems
static esp_err_t copy_file(const char *source_path, const char *dest_path, transfer_progress_t *progress) {
    ESP_LOGI(TAG, "Copying from %s to %s", source_path, dest_path);
    FILE *source_file = fopen(source_path, "rb");
    if (!source_file) {
        ESP_LOGE(TAG, "Failed to open source file: %s", source_path);
        if (progress) snprintf(progress->error_msg, sizeof(progress->error_msg), "Failed to open source file.");
        return ESP_FAIL;
    }

    // Get file size for progress tracking
    struct stat st;
    if (stat(source_path, &st) == 0) {
        if(progress) progress->total_bytes = st.st_size;
    } else {
        if(progress) progress->total_bytes = 0;
    }
    if(progress) progress->bytes_transferred = 0;


    FILE *dest_file = fopen(dest_path, "wb");
    if (!dest_file) {
        ESP_LOGE(TAG, "Failed to open destination file: %s", dest_path);
        if (progress) snprintf(progress->error_msg, sizeof(progress->error_msg), "Failed to open destination file.");
        fclose(source_file);
        return ESP_FAIL;
    }

    // Increase buffer size for faster copies
    char *buffer = malloc(4096);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for copy buffer");
        if (progress) snprintf(progress->error_msg, sizeof(progress->error_msg), "Memory allocation failed.");
        fclose(source_file);
        fclose(dest_file);
        return ESP_FAIL;
    }

    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, 4096, source_file)) > 0) {
        if (g_cancel_transfer) {
            ESP_LOGW(TAG, "Transfer cancelled by user.");
            snprintf(progress->error_msg, sizeof(progress->error_msg), "Transfer cancelled.");
            free(buffer);
            fclose(source_file);
            fclose(dest_file);
            // Optionally delete the partially copied file
            remove(dest_path);
            return ESP_FAIL;
        }
        if (fwrite(buffer, 1, bytes_read, dest_file) != bytes_read) {
            ESP_LOGE(TAG, "Failed to write to destination file");
            if (progress) snprintf(progress->error_msg, sizeof(progress->error_msg), "Write error on destination.");
            free(buffer);
            fclose(source_file);
            fclose(dest_file);
            return ESP_FAIL;
        }
        if (progress) {
            progress->bytes_transferred += bytes_read;
        }
    }
    
    free(buffer);
    fclose(source_file);
    fclose(dest_file);
    ESP_LOGI(TAG, "File copied successfully");
    if (progress) progress->success = true;
    return ESP_OK;
}

// --- WEB SERVER HANDLERS ---
// Generic handler for serving static files from SPIFFS
static esp_err_t static_file_handler(httpd_req_t *req) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s%s", MOUNT_POINT_SPIFFS, req->uri);

    // Default to index.html if root is requested
    if (strcmp(req->uri, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "%s/index.html", MOUNT_POINT_SPIFFS);
    }

    struct stat path_stat;
    if (stat(filepath, &path_stat) == -1) {
        ESP_LOGE(TAG, "File not found: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Set content type based on file extension
    const char *type = "text/plain";
    if (strstr(filepath, ".html")) type = "text/html";
    else if (strstr(filepath, ".css")) type = "text/css";
    else if (strstr(filepath, ".js")) type = "application/javascript";
    httpd_resp_set_type(req, type);

    // Open and send file
    FILE *fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", filepath);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char *chunk = malloc(1024);
    size_t chunk_size;
    do {
        chunk_size = fread(chunk, 1, 1024, fd);
        if (chunk_size > 0) {
            if (httpd_resp_send_chunk(req, chunk, chunk_size) != ESP_OK) {
                fclose(fd);
                free(chunk);
                ESP_LOGE(TAG, "File sending failed!");
                return ESP_FAIL;
            }
        }
    } while (chunk_size > 0);

    httpd_resp_send_chunk(req, NULL, 0); // End response
    fclose(fd);
    free(chunk);
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "reader_connected", ebook_reader_connected);
    cJSON_AddBoolToObject(root, "transfer_active", g_transfer_progress.active);
    if (g_transfer_progress.active) {
        cJSON_AddStringToObject(root, "filename", g_transfer_progress.filename);
        cJSON_AddNumberToObject(root, "bytes_transferred", g_transfer_progress.bytes_transferred);
        cJSON_AddNumberToObject(root, "total_bytes", g_transfer_progress.total_bytes);
    }
    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t list_files_handler(httpd_req_t *req) {
    char buf[128];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) != ESP_OK) {
        httpd_resp_send_400(req);
        return ESP_FAIL;
    }

    char param[32];
    if (httpd_query_key_value(buf, "type", param, sizeof(param)) != ESP_OK) {
        httpd_resp_send_400(req);
        return ESP_FAIL;
    }

    const char *mount_path = (strcmp(param, "sd") == 0) ? MOUNT_POINT_SD : MOUNT_POINT_USB;
    
    if (strcmp(param, "usb") == 0 && !ebook_reader_connected) {
         httpd_resp_set_type(req, "application/json");
         httpd_resp_send(req, "[]", 2);
         return ESP_OK;
    }

    DIR *d = opendir(mount_path);
    if (!d) {
        ESP_LOGE(TAG, "Failed to open directory: %s", mount_path);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateArray();
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (dir->d_type == DT_REG) { // If it's a regular file
            // Filter for common ebook formats
            if (strstr(dir->d_name, ".epub") || strstr(dir->d_name, ".mobi") || strstr(dir->d_name, ".pdf") || strstr(dir->d_name, ".txt")) {
                cJSON *file_obj = cJSON_CreateObject();
                cJSON_AddStringToObject(file_obj, "name", dir->d_name);
                cJSON_AddItemToArray(root, file_obj);
            }
        }
    }
    closedir(d);

    httpd_resp_set_type(req, "application/json");
    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    cJSON_Delete(root);

    return ESP_OK;
}

static esp_err_t transfer_file_handler(httpd_req_t *req) {
    if (g_transfer_progress.active) {
        httpd_resp_send_err(req, HTTPD_429_TOO_MANY_REQUESTS, "A file transfer is already in progress.");
        return ESP_FAIL;
    }

    g_led_state = LED_STATE_TRANSFER;

    char content[256];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        g_led_state = ebook_reader_connected ? LED_STATE_CONNECTED : LED_STATE_IDLE;
        return ESP_FAIL;
    }
    content[ret] = '\0';

    cJSON *json = cJSON_Parse(content);
    if (!json) {
        httpd_resp_send_400(req);
        g_led_state = ebook_reader_connected ? LED_STATE_CONNECTED : LED_STATE_IDLE;
        return ESP_FAIL;
    }

    const char *source = cJSON_GetObjectItem(json, "source")->valuestring;
    const char *destination = cJSON_GetObjectItem(json, "destination")->valuestring;
    const char *filename = cJSON_GetObjectItem(json, "filename")->valuestring;

    if (!source || !destination || !filename) {
        cJSON_Delete(json);
        httpd_resp_send_400(req);
        g_led_state = ebook_reader_connected ? LED_STATE_CONNECTED : LED_STATE_IDLE;
        return ESP_FAIL;
    }

    // Initialize progress tracking
    g_transfer_progress.active = true;
    g_cancel_transfer = false; // Reset cancel flag
    strlcpy(g_transfer_progress.filename, filename, sizeof(g_transfer_progress.filename));
    g_transfer_progress.bytes_transferred = 0;
    g_transfer_progress.total_bytes = 0;
    g_transfer_progress.success = false;
    g_transfer_progress.error_msg[0] = '\0';

    char source_path[256];
    char dest_path[256];

    snprintf(source_path, sizeof(source_path), "%s/%s", (strcmp(source, "sd") == 0) ? MOUNT_POINT_SD : MOUNT_POINT_USB, filename);
    snprintf(dest_path, sizeof(dest_path), "%s/%s", (strcmp(destination, "sd") == 0) ? MOUNT_POINT_SD : MOUNT_POINT_USB, filename);
    
    esp_err_t res = copy_file(source_path, dest_path, &g_transfer_progress);
    
    cJSON_Delete(json);

    // Set LED state back based on connection status
    g_led_state = ebook_reader_connected ? LED_STATE_CONNECTED : LED_STATE_IDLE;
    if (res != ESP_OK) {
        g_led_state = LED_STATE_ERROR; // Indicate error on LED
    }


    cJSON *response_json = cJSON_CreateObject();
    cJSON_AddBoolToObject(response_json, "success", res == ESP_OK);
    cJSON_AddStringToObject(response_json, "message", res == ESP_OK ? "File transfer complete!" : g_transfer_progress.error_msg);
    
    char *json_str = cJSON_PrintUnformatted(response_json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    cJSON_Delete(response_json);

    // Deactivate progress tracking after sending response
    g_transfer_progress.active = false;

    return ESP_OK;
}

static esp_err_t transfer_cancel_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Received request to cancel transfer");
    g_cancel_transfer = true;
    httpd_resp_send(req, "OK", HTTPD_200_OK);
    return ESP_OK;
}

// Handler to report the current file transfer progress
static esp_err_t transfer_progress_handler(httpd_req_t *req) {
    if (!g_transfer_progress.active) {
        httpd_resp_send_404(req);
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "filename", g_transfer_progress.filename);
    cJSON_AddNumberToObject(root, "bytes_transferred", g_transfer_progress.bytes_transferred);
    cJSON_AddNumberToObject(root, "total_bytes", g_transfer_progress.total_bytes);

    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    cJSON_Delete(root);

    return ESP_OK;
}


// --- WEB SERVER SETUP ---
static httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Handlers for dynamic content
        httpd_uri_t status_uri = { "/status", HTTP_GET, status_handler, NULL };
        httpd_register_uri_handler(server, &status_uri);
        
        httpd_uri_t list_uri = { "/list-files", HTTP_GET, list_files_handler, NULL };
        httpd_register_uri_handler(server, &list_uri);
        
        httpd_uri_t transfer_uri = { "/transfer-file", HTTP_POST, transfer_file_handler, NULL };
        httpd_register_uri_handler(server, &transfer_uri);

        httpd_uri_t progress_uri = { "/transfer-progress", HTTP_GET, transfer_progress_handler, NULL };
        httpd_register_uri_handler(server, &progress_uri);

        httpd_uri_t cancel_uri = { "/transfer-cancel", HTTP_POST, transfer_cancel_handler, NULL };
        httpd_register_uri_handler(server, &cancel_uri);

        // Handler for all other URIs (serves static files)
        httpd_uri_t static_uri = { "/*", HTTP_GET, static_file_handler, NULL };
        httpd_register_uri_handler(server, &static_uri);
    }
    return server;
}

// --- WI-FI AP SETUP ---
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

void init_wifi_ap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .ssid_len = strlen(WIFI_SSID),
            .channel = 1,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .max_connection = WIFI_MAX_STA_CONN,
        },
    };
    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP initialized. SSID:%s password:%s", WIFI_SSID, WIFI_PASS);
}

// --- SPIFFS SETUP ---
void init_spiffs(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = MOUNT_POINT_SPIFFS,
      .partition_label = "storage",
      .max_files = 5,
      .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        g_led_state = LED_STATE_ERROR;
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        g_led_state = LED_STATE_ERROR;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
}

// --- SD CARD SETUP ---
void init_sd_card(void) {
    esp_err_t ret;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT_SD;
    ESP_LOGI(TAG, "Initializing SD card");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize spi bus.");
        g_led_state = LED_STATE_ERROR;
        return;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card VFS");
        g_led_state = LED_STATE_ERROR;
    } else {
        ESP_LOGI(TAG, "SD card mounted successfully at %s", mount_point);
    }
}

// --- USB HOST SETUP ---
static void msc_event_cb(const msc_host_event_t *event, void *arg)
{
    if (event->event == MSC_DEVICE_CONNECTED) {
        ESP_LOGI(TAG, "MSC device connected");
        ebook_reader_connected = true;
        g_led_state = LED_STATE_CONNECTED;
        ESP_ERROR_CHECK(msc_host_install_device(event->device, &device_handle));
        
        // Mount the filesystem
        ESP_ERROR_CHECK(vfs_msc_mount(MOUNT_POINT_USB, device_handle));
        ESP_LOGI(TAG, "MSC device mounted at %s", MOUNT_POINT_USB);

    } else if (event->event == MSC_DEVICE_DISCONNECTED) {
        ESP_LOGI(TAG, "MSC device disconnected");
        ebook_reader_connected = false;
        g_led_state = LED_STATE_IDLE;
        // Unmount the filesystem
        vfs_msc_unmount(MOUNT_POINT_USB);
        ESP_LOGI(TAG, "MSC device unmounted");
        msc_host_uninstall_device(device_handle);
    }
}

void usb_host_lib_task(void *arg)
{
    while (1) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
             ESP_LOGI(TAG, "USB host free, terminating task");
        }
    }
     vTaskDelete(NULL);
}

void init_usb_host() {
    ESP_LOGI(TAG, "Installing USB Host Library");
    const usb_host_config_t host_config = {
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));
    
    // Create a task to handle USB library events
    xTaskCreate(usb_host_lib_task, "usb_host", 4096, NULL, 10, NULL);

    ESP_LOGI(TAG, "Installing MSC client");
    const msc_host_driver_config_t msc_config = {
        .create_backround_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .callback = msc_event_.cb,
    };
    ESP_ERROR_CHECK(msc_host_install(&msc_config));
}

// --- LED STRIP ---
static void led_status_task(void *pvParameters) {
    uint8_t brightness = 0;
    bool increasing = true;

    while (1) {
        switch (g_led_state) {
            case LED_STATE_IDLE: // Pulsing Blue
                if (increasing) {
                    brightness += 2;
                    if (brightness >= 100) increasing = false;
                } else {
                    brightness -= 2;
                    if (brightness <= 0) increasing = true;
                }
                for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
                    led_strip_set_pixel(g_led_strip, i, 0, 0, brightness);
                }
                led_strip_refresh(g_led_strip);
                vTaskDelay(pdMS_TO_TICKS(20));
                break;

            case LED_STATE_CONNECTED: // Solid Green
                for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
                    led_strip_set_pixel(g_led_strip, i, 0, 128, 0); // Green
                }
                led_strip_refresh(g_led_strip);
                vTaskDelay(pdMS_TO_TICKS(500)); // Sleep to prevent busy-looping
                break;

            case LED_STATE_TRANSFER: // Pulsing White
                 if (increasing) {
                    brightness += 5;
                    if (brightness >= 150) increasing = false;
                } else {
                    brightness -= 5;
                    if (brightness <= 0) increasing = true;
                }
                for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
                    led_strip_set_pixel(g_led_strip, i, brightness, brightness, brightness);
                }
                led_strip_refresh(g_led_strip);
                vTaskDelay(pdMS_TO_TICKS(15));
                break;

            case LED_STATE_ERROR: // Solid Red
                for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
                    led_strip_set_pixel(g_led_strip, i, 128, 0, 0); // Red
                }
                led_strip_refresh(g_led_strip);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
            
            default:
                 vTaskDelay(pdMS_TO_TICKS(100));
                 break;
        }
    }
}

void init_led_strip(void) {
    ESP_LOGI(TAG, "Create RMT TX channel");
    rmt_channel_handle_t led_chan = NULL;
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = LED_STRIP_GPIO,
        .mem_block_symbols = 64, // Increase if needed
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

    ESP_LOGI(TAG, "Install WS2812 driver");
    led_strip_encoder_config_t encoder_config = {
        .resolution = LED_STRIP_RMT_RES_HZ,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_encoder(&encoder_config, &g_led_strip));
    
    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(led_chan));
    
    led_strip_clear(g_led_strip);
}


// --- MAIN APPLICATION ENTRY POINT ---
void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize subsystems
    init_spiffs();
    init_sd_card();
    init_wifi_ap();
    init_led_strip();
    init_usb_host();

    // Start background tasks
    xTaskCreate(led_status_task, "led_status_task", 2048, NULL, 5, NULL);
    
    // Start the web server
    start_webserver();

    ESP_LOGI(TAG, "E-Book Librarian is running!");
    g_led_state = LED_STATE_IDLE; // Set initial state after setup is complete
}
