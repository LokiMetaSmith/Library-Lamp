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
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <math.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "errno.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_http_server.h"
#include "esp_vfs_fat.h"
#include "esp_spiffs.h"
#include "sdmmc_cmd.h"
#include "cJSON.h"
#include "lorawan.h"
#include "bulletin_board.h"
#include "bulletin_api.h"
#include "audio_api.h"

// --- Local Dependencies ---
#include "dns_server.h"

// --- USB Host Dependencies (from the official ESP-IDF stack) ---
#include "usb/usb_host.h"
#include "usb/msc_host_vfs.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_sleep.h"
#include "usb/usb_host.h"
#include "usb/msc_host.h"
#include "usb/msc_host_vfs.h"
#include "vfs_fat_internal.h"

// --- LED Strip Dependencies ---
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "led_strip.h"
////#include "miniz.h"
#include "sqlite3.h"

// --- Bluetooth Dependencies ---
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"


// --- CONFIGURATION ---
// Wi-Fi AP fallback settings
#define WIFI_AP_SSID      "Ebook-Library-Box-Setup"
#define WIFI_AP_PASS      "" // Open network for setup
#define WIFI_AP_MAX_STA_CONN 4

// NVS Storage Keys
#define NVS_NAMESPACE "wifi_creds"
#define NVS_KEY_PUBLIC_UPLOADS "pub_upload"
#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PASS "password"
#define NVS_KEY_LED_R "led_r"
#define NVS_KEY_LED_G "led_g"
#define NVS_KEY_LED_B "led_b"


// SD Card mount point and pin configuration
#define MOUNT_POINT_SD "/sdcard"
#ifdef CONFIG_SD_MISO_PIN
#define PIN_NUM_MISO CONFIG_SD_MISO_PIN
#else
#define PIN_NUM_MISO 10
#endif

#ifdef CONFIG_SD_MOSI_PIN
#define PIN_NUM_MOSI CONFIG_SD_MOSI_PIN
#else
#define PIN_NUM_MOSI 11
#endif

#ifdef CONFIG_SD_CLK_PIN
#define PIN_NUM_CLK  CONFIG_SD_CLK_PIN
#else
#define PIN_NUM_CLK  12
#endif

#ifdef CONFIG_SD_CS_PIN
#define PIN_NUM_CS   CONFIG_SD_CS_PIN
#else
#define PIN_NUM_CS   13
#endif

// USB mount point
#define MOUNT_POINT_USB "/usb"

// SPIFFS mount point for web assets
#define MOUNT_POINT_SPIFFS "/spiffs"


// LED Strip configuration
#ifdef CONFIG_LED_STRIP_GPIO
#define LED_STRIP_GPIO              CONFIG_LED_STRIP_GPIO
#else
#define LED_STRIP_GPIO              21
#endif

#define LED_STRIP_LED_NUMBERS       8
#define LED_STRIP_RMT_RES_HZ        (10 * 1000 * 1000) // 10MHz resolution

// Eject Button
#ifdef CONFIG_EJECT_BUTTON_GPIO
#define EJECT_BUTTON_GPIO           CONFIG_EJECT_BUTTON_GPIO
#else
#define EJECT_BUTTON_GPIO           0
#endif

#define HOST_LIB_TASK_PRIORITY    2
#define CLASS_TASK_PRIORITY     3

#define APP_QUIT_PIN 0

static SemaphoreHandle_t ws_mutex;

#ifdef CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK
#define ENABLE_ENUM_FILTER_CALLBACK
#endif // CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK

// // extern void class_driver_task(void *arg);
extern void class_driver_client_deregister(void);



// --- GLOBALS ---
static const char *TAG = "EBOOK_LIBRARIAN";

// --- Async Background Queues & State ---
QueueHandle_t catalog_update_queue = NULL;
volatile bool catalog_updating = false;

typedef struct {
    char filename[100];
    char title[100];
    char author[100];
} catalog_update_msg_t;

// Background task to write to catalog.json
void catalog_update_task(void *pvParameters) {
    catalog_update_msg_t msg;
    while (1) {
        if (xQueueReceive(catalog_update_queue, &msg, portMAX_DELAY) == pdPASS) {
            catalog_updating = true;
            ESP_LOGI(TAG, "Background processing catalog update for: %s", msg.filename);

            char catalog_path[256];
            snprintf(catalog_path, sizeof(catalog_path), "%s/catalog.json", MOUNT_POINT_SD);
            FILE *cat_f = fopen(catalog_path, "r");
            cJSON *cat_json = NULL;

            if (cat_f) {
                fseek(cat_f, 0, SEEK_END);
                long fsize = ftell(cat_f);
                fseek(cat_f, 0, SEEK_SET);
                if (fsize > 0) {
                    char *json_data = malloc(fsize + 1);
                    if (json_data) {
                        fread(json_data, 1, fsize, cat_f);
                        json_data[fsize] = 0;
                        cat_json = cJSON_Parse(json_data);
                        free(json_data);
                    }
                }
                fclose(cat_f);
            }

            if (!cat_json) cat_json = cJSON_CreateArray();

            cJSON *new_book = cJSON_CreateObject();
            cJSON_AddStringToObject(new_book, "name", msg.filename);
            cJSON_AddStringToObject(new_book, "title", msg.title);
            cJSON_AddStringToObject(new_book, "author", msg.author);
            cJSON_AddItemToArray(cat_json, new_book);

            cat_f = fopen(catalog_path, "w");
            if (cat_f) {
                char *new_json_str = cJSON_PrintUnformatted(cat_json);
                if (new_json_str) {
                    fwrite(new_json_str, 1, strlen(new_json_str), cat_f);
                    free(new_json_str);
                }
                fclose(cat_f);
            }
            cJSON_Delete(cat_json);

            // Broadcast the availability over LoRaWAN now that the catalog is updated
            #ifdef LORA_USE_SX1262
            char lora_msg[128];
            snprintf(lora_msg, sizeof(lora_msg), "Library active: catalog.json available.");
            lora_wan_broadcast(lora_msg);
            #endif

            catalog_updating = false;
        }
    }
}

// --- GLOBALS ---
static bool ebook_reader_connected = false;
static msc_host_device_handle_t device_handle = NULL;
static msc_host_vfs_handle_t vfs_handle = NULL;
static led_strip_handle_t g_led_strip;
static bool g_wifi_configured = false;
bool allow_public_uploads = false;
bool g_usb_mounted = false;
bool g_sd_card_initialized = false;
sdmmc_card_t *g_sd_card_handle = NULL;
extern bool lora_initialized;
QueueHandle_t app_event_queue = NULL;

/**
 * @brief APP event group
 *
 * Application logic can be different. There is a one among other ways to distinguish the
 * event by application event group.
 * In this example we have two event groups:
 * APP_EVENT            - General event, which is APP_QUIT_PIN press event (Generally, it is IO0).
 * APP_EVENT_HID_HOST   - HID Host Driver event, such as device connection/disconnection or input report.
 */
 
typedef enum {
    APP_EVENT = 0,
    APP_EVENT_HID_HOST
} app_event_group_t;

/**
 * @brief APP event queue
 *
 * This event is used for delivering the HID Host event from callback to a task.
 */
typedef struct {
    app_event_group_t event_group;
    /* HID Host - Device related info */
    struct {

    } hid_host_device;
} app_event_queue_t;


// Event group to signal Wi-Fi connection events
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_FAIL_BIT = BIT1;

// Embedded HTML for setup page
#include "setup_html.h"


// Enum and global variable for managing LED state
typedef enum {
    LED_STATE_INIT,
    LED_STATE_IDLE,
    LED_STATE_CONNECTED,
    LED_STATE_TRANSFER,
    LED_STATE_ERROR,
    LED_STATE_SETUP, // New state for config mode
    LED_STATE_EJECT, // For eject button feedback
    LED_STATE_WAIT_CONFIRM,
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
volatile bool g_transfer_confirmed = false;
volatile bool g_waiting_for_transfer_confirm = false;

// Global LED colors
uint8_t g_led_color_r = 255;
uint8_t g_led_color_g = 255;
uint8_t g_led_color_b = 255;


// --- NVS Functions ---
esp_err_t save_wifi_credentials(const char *ssid, const char *password) {
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(my_handle, NVS_KEY_SSID, ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write SSID to NVS: %s", esp_err_to_name(err));
        nvs_close(my_handle);
        return err;
    }

    err = nvs_set_str(my_handle, NVS_KEY_PASS, password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write Password to NVS: %s", esp_err_to_name(err));
        nvs_close(my_handle);
        return err;
    }

    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Wi-Fi credentials saved to NVS");
    }

    nvs_close(my_handle);
    return err;
}

void load_led_color_from_nvs(void) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        uint8_t r, g, b;
        if (nvs_get_u8(my_handle, NVS_KEY_LED_R, &r) == ESP_OK) g_led_color_r = r;
        if (nvs_get_u8(my_handle, NVS_KEY_LED_G, &g) == ESP_OK) g_led_color_g = g;
        if (nvs_get_u8(my_handle, NVS_KEY_LED_B, &b) == ESP_OK) g_led_color_b = b;
        nvs_close(my_handle);
    } else {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle to load LED color!", esp_err_to_name(err));
    }
}

void save_led_color_to_nvs(uint8_t r, uint8_t g, uint8_t b) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        nvs_set_u8(my_handle, NVS_KEY_LED_R, r);
        nvs_set_u8(my_handle, NVS_KEY_LED_G, g);
        nvs_set_u8(my_handle, NVS_KEY_LED_B, b);

        err = nvs_commit(my_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit LED NVS changes: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "LED color saved to NVS: %d, %d, %d", r, g, b);
        }
        nvs_close(my_handle);
    } else {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle to save LED color!", esp_err_to_name(err));
    }
}

esp_err_t load_wifi_credentials(char *ssid, size_t ssid_len, char *password, size_t pass_len) {
    nvs_handle_t my_handle;
    esp_err_t err;


    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "NVS namespace '%s' not found. First boot?", NVS_NAMESPACE);
        } else {
            ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        }
        return err;
    }

    uint8_t u_val = 0;
    if (nvs_get_u8(my_handle, NVS_KEY_PUBLIC_UPLOADS, &u_val) == ESP_OK) {
        allow_public_uploads = (u_val == 1);
    }


    err = nvs_get_str(my_handle, NVS_KEY_SSID, ssid, &ssid_len);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "SSID not found in NVS");
        nvs_close(my_handle);
        return err;
    }

    err = nvs_get_str(my_handle, NVS_KEY_PASS, password, &pass_len);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Password not found in NVS");
        // This is not a fatal error, password can be empty
    }

    nvs_close(my_handle);
    return ESP_OK;
}


// --- HELPER FUNCTIONS ---

// Simple helper to extract content from an XML tag.
// NOTE: This is a very basic parser and will not handle complex XML,
// but it's sufficient for the simple structure of OPF files.


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

// --- WEB SERVER HANDLERS (CAPTIVE PORTAL) ---
// Handler to serve the setup page

static esp_err_t delete_file_handler(httpd_req_t *req) {
    if (!bb_is_admin_request(req)) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Forbidden");
        return ESP_FAIL;
    }

    char content[256];
    if (req->content_len >= sizeof(content)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
        return ESP_FAIL;
    }
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        return ESP_FAIL;
    }
    content[ret] = 0;

    cJSON *json = cJSON_Parse(content);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }

    cJSON *j_source = cJSON_GetObjectItem(json, "source");
    cJSON *j_filename = cJSON_GetObjectItem(json, "filename");

    if (!j_source || !cJSON_IsString(j_source) || !j_filename || !cJSON_IsString(j_filename)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }

    const char *source = j_source->valuestring;
    const char *filename = j_filename->valuestring;

    if (strstr(filename, "..") != NULL || strchr(filename, '/') != NULL) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Filename");
        return ESP_FAIL;
    }

    if (strcasestr(filename, "adminkey.json") != NULL || strcasestr(filename, ".key") != NULL) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Forbidden");
        return ESP_FAIL;
    }

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", (strcmp(source, "sd") == 0) ? MOUNT_POINT_SD : MOUNT_POINT_USB, filename);
    cJSON_Delete(json);

    if (remove(filepath) == 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\": true}", 17);
        return ESP_OK;
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to delete file");
        return ESP_FAIL;
    }
}

static esp_err_t setup_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Serving setup page");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char*)main_web_assets_setup_html, main_web_assets_setup_html_len);
    return ESP_OK;
}

// Handler to save credentials and restart
static esp_err_t save_credentials_post_handler(httpd_req_t *req) {
    char buf[128];
    int ret;
    size_t remaining = req->content_len;

    if (remaining > sizeof(buf) -1) {
        ESP_LOGE(TAG, "Content too long");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char ssid[32] = {0};
    char password[64] = {0};

    if (httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid)) != ESP_OK ||
        httpd_query_key_value(buf, "password", password, sizeof(password)) != ESP_OK) {
        ESP_LOGE(TAG, "Could not parse ssid/password from POST data: %s", buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Received SSID: %s", ssid);
    // Do not log password for security reasons

    esp_err_t err = save_wifi_credentials(ssid, password);
    if (err != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Respond before restarting
    httpd_resp_send(req, "Wi-Fi credentials saved. The device will now restart.", HTTPD_RESP_USE_STRLEN);

    // Restart the device to apply the new settings
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

// --- WEB SERVER HANDLERS (MAIN APP) ---
static esp_err_t static_file_handler(httpd_req_t *req) {

    // Prevent path traversal
    if (strstr(req->uri, "..") != NULL) {
        ESP_LOGE(TAG, "Path traversal attempt detected: %s", req->uri);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid URI");
        return ESP_FAIL;
    }

    // Prevent access to sensitive files
    if (strcasestr(req->uri, "adminkey.json") != NULL || strcasestr(req->uri, ".key") != NULL) {
        ESP_LOGE(TAG, "Attempted access to protected file: %s", req->uri);
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Forbidden");
        return ESP_FAIL;
    }

    // If we're hitting /generate_204 or other known captive portal probe endpoints, redirect them to root
    if (strstr(req->uri, "generate_204") != NULL || strstr(req->uri, "hotspot-detect") != NULL) {
        httpd_resp_set_status(req, "302 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "/");
        httpd_resp_send(req, "Redirecting to main page", HTTPD_RESP_USE_STRLEN);
        ESP_LOGI(TAG, "Redirecting to root for captive portal");
        return ESP_OK;
    }

    // Detect e-readers and serve ereader.html by default instead of index.html
    if (strcmp(req->uri, "/") == 0) {
        char user_agent[256];
        if (httpd_req_get_hdr_value_str(req, "User-Agent", user_agent, sizeof(user_agent)) == ESP_OK) {
            // Convert user agent to lower case for easier matching
            for(int i = 0; user_agent[i]; i++) {
                user_agent[i] = tolower((unsigned char)user_agent[i]);
            }
            if (strstr(user_agent, "kobo") || strstr(user_agent, "kindle") || strstr(user_agent, "tolino") || strstr(user_agent, "ereader")) {
                ESP_LOGI(TAG, "E-Reader detected, redirecting to /ereader.html");
                httpd_resp_set_status(req, "302 Temporary Redirect");
                httpd_resp_set_hdr(req, "Location", "/ereader.html");
                httpd_resp_send(req, "Redirecting", HTTPD_RESP_USE_STRLEN);
                return ESP_OK;
            }
        }
    }
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s%.*s", MOUNT_POINT_SPIFFS, (int)(sizeof(filepath) - strlen(MOUNT_POINT_SPIFFS) - 1), req->uri);

    // Default to index.html if root is requested
    if (strcmp(req->uri, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "%s/index.html", MOUNT_POINT_SPIFFS);
    }

    struct stat path_stat;
    if (stat(filepath, &path_stat) == -1) {
        ESP_LOGE(TAG, "File not found: %s", filepath);

        // Prevent infinite redirect loops for core endpoints
        if (strcmp(req->uri, "/") == 0 || strcmp(req->uri, "/index.html") == 0 || strcmp(req->uri, "/ereader.html") == 0 || strcmp(req->uri, "/favicon.ico") == 0) {
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }

        // Instead of 404, redirect to root for captive portal functionality
        httpd_resp_set_status(req, "302 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "/");
        httpd_resp_send(req, "Redirecting to main page", HTTPD_RESP_USE_STRLEN);
        ESP_LOGI(TAG, "Redirecting to root for captive portal");
        return ESP_OK;
    }

    // Set content type based on file extension
    const char *type = "text/plain";
    const char *ext = strrchr(filepath, '.');
    if (ext) {
        if (strcasecmp(ext, ".html") == 0) type = "text/html";
        else if (strcasecmp(ext, ".css") == 0) type = "text/css";
        else if (strcasecmp(ext, ".js") == 0) type = "application/javascript";
    }
    httpd_resp_set_type(req, type);

    // Open and send file
    FILE *fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", filepath);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char *chunk = malloc(4096);
    size_t chunk_size;
    do {
        chunk_size = fread(chunk, 1, 4096, fd);
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



// Simple URL decode function
void urldecode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit((unsigned char)a) && isxdigit((unsigned char)b))) {
            if (a >= 'a')
                    a -= 'a'-'A';
            if (a >= 'A')
                    a -= ('A' - 10);
            else
                    a -= '0';
            if (b >= 'a')
                    b -= 'a'-'A';
            if (b >= 'A')
                    b -= ('A' - 10);
            else
                    b -= '0';
            *dst++ = 16*a+b;
            src+=3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst++ = '\0';
}

// --- Download File Handler ---


static esp_err_t upload_handler(httpd_req_t *req) {
    if (!allow_public_uploads && !bb_is_admin_request(req)) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Public uploads are disabled.");
        return ESP_FAIL;
    }

    // Since multipart/form-data can be complex to parse perfectly without a library,
    // and ebook files can be large, we'll implement a basic parser that works for our specific JS client.
    // The client will send title, author, and file via FormData.

    // A more robust approach for large files is receiving the whole body, but ESP32 memory is limited.
    // We expect the client to send 'title', 'author', and 'file' using a simple custom JSON + Binary approach
    // or we parse the multipart headers manually.

    // Actually, to make things easy and robust, the frontend can send a JSON metadata packet first,
    // or send the file directly in a POST, with title/author in URL query parameters!
    // That avoids memory-intensive multipart parsing on the ESP32.

    char title[100] = "Unknown Title";
    char author[100] = "Unknown Author";
    char filename[100] = "upload.epub";

    size_t query_len = httpd_req_get_url_query_len(req);
    if (query_len > 0) {
        char *buf = malloc(query_len + 1);
        if (buf) {
            if (httpd_req_get_url_query_str(req, buf, query_len + 1) == ESP_OK) {
                char param[100];
                if (httpd_query_key_value(buf, "title", param, sizeof(param)) == ESP_OK) urldecode(title, param);
                if (httpd_query_key_value(buf, "author", param, sizeof(param)) == ESP_OK) urldecode(author, param);
                if (httpd_query_key_value(buf, "filename", param, sizeof(param)) == ESP_OK) urldecode(filename, param);
            }
            free(buf);
        }
    }

    // Check for directory traversal
    if (strstr(filename, "..") || strchr(filename, '/')) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid filename.");
        return ESP_FAIL;
    }

    // Prevent overwriting sensitive files
    if (strcasestr(filename, "adminkey.json") != NULL || strcasestr(filename, ".key") != NULL) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Forbidden");
        return ESP_FAIL;
    }

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT_SD, filename);

    FILE *f = fopen(filepath, "w");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file on SD card.");
        return ESP_FAIL;
    }

    // Receive the file chunk by chunk
    char *recv_buf = malloc(4096);
    if (!recv_buf) {
        fclose(f);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed.");
        return ESP_FAIL;
    }

        size_t remaining = req->content_len;

    while (remaining > 0) {
        int ret = httpd_req_recv(req, recv_buf, MIN(remaining, 4096));
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            fclose(f);
            free(recv_buf);
            remove(filepath); // delete incomplete file
            return ESP_FAIL;
        }
        fwrite(recv_buf, 1, ret, f);
        remaining -= ret;
    }

    fclose(f);
    free(recv_buf);

    // Update catalog.json (ASYNC via FreeRTOS Task)
    if (catalog_update_queue != NULL) {
        catalog_update_msg_t msg;
        strncpy(msg.filename, filename, sizeof(msg.filename) - 1);
        msg.filename[sizeof(msg.filename) - 1] = '\0';
        strncpy(msg.title, title, sizeof(msg.title) - 1);
        msg.title[sizeof(msg.title) - 1] = '\0';
        strncpy(msg.author, author, sizeof(msg.author) - 1);
        msg.author[sizeof(msg.author) - 1] = '\0';

        if (xQueueSend(catalog_update_queue, &msg, pdMS_TO_TICKS(100)) != pdPASS) {
            ESP_LOGE(TAG, "Failed to enqueue catalog update (queue full)");
        }
    } else {
         ESP_LOGE(TAG, "catalog_update_queue not initialized!");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true}", 16);
    return ESP_OK;
}

static esp_err_t download_handler(httpd_req_t *req) {
    size_t query_len = httpd_req_get_url_query_len(req);
    if (query_len == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }

    char *buf = malloc(query_len + 1);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    if (httpd_req_get_url_query_str(req, buf, query_len + 1) != ESP_OK) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }

    char file_param[128];
    char source_param[32];

    if (httpd_query_key_value(buf, "file", file_param, sizeof(file_param)) != ESP_OK ||
        httpd_query_key_value(buf, "source", source_param, sizeof(source_param)) != ESP_OK) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }
    free(buf);

    char decoded_file_param[128];
    urldecode(decoded_file_param, file_param);


    const char *mount_path = (strcmp(source_param, "sd") == 0) ? MOUNT_POINT_SD : MOUNT_POINT_USB;

    if (strcmp(source_param, "usb") == 0 && !ebook_reader_connected) {
         httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "USB Not Connected");
         return ESP_FAIL;
    }

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", mount_path, decoded_file_param);

    // Basic path traversal prevention
    if (strstr(decoded_file_param, "..") != NULL || strchr(decoded_file_param, '/') != NULL || strchr(decoded_file_param, '\\') != NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Filename");
        return ESP_FAIL;
    }

    // Prevent access to sensitive files
    if (strcasestr(decoded_file_param, "adminkey.json") != NULL || strcasestr(decoded_file_param, ".key") != NULL) {
        ESP_LOGE(TAG, "Attempted access to protected file: %s", decoded_file_param);
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Forbidden");
        return ESP_FAIL;
    }

    struct stat path_stat;
    if (stat(filepath, &path_stat) == -1) {
        ESP_LOGE(TAG, "File not found for download: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Determine content type based on extension
    const char *type = "application/octet-stream";
    const char *ext = strrchr(filepath, '.');
    if (ext) {
        if (strcasecmp(ext, ".epub") == 0) type = "application/epub+zip";
        else if (strcasecmp(ext, ".mobi") == 0) type = "application/x-mobipocket-ebook";
        else if (strcasecmp(ext, ".pdf") == 0) type = "application/pdf";
        else if (strcasecmp(ext, ".txt") == 0) type = "text/plain";
        else if (strcasecmp(ext, ".mp3") == 0) type = "audio/mpeg";
        else if (strcasecmp(ext, ".m4a") == 0) type = "audio/mp4";
        else if (strcasecmp(ext, ".wav") == 0) type = "audio/wav";
        else if (strcasecmp(ext, ".ogg") == 0) type = "audio/ogg";
    }

    httpd_resp_set_type(req, type);
    httpd_resp_set_hdr(req, "Accept-Ranges", "bytes");

    if (!strstr(type, "audio/")) {
        char disposition_header[256];
        snprintf(disposition_header, sizeof(disposition_header), "attachment; filename=\"%s\"", decoded_file_param);
        httpd_resp_set_hdr(req, "Content-Disposition", disposition_header);
    }

    FILE *fd = fopen(filepath, "rb");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to open file for download: %s", filepath);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    size_t file_size = path_stat.st_size;
    size_t start_byte = 0;
    size_t end_byte = file_size - 1;

    char range_header[64];
    if (httpd_req_get_hdr_value_str(req, "Range", range_header, sizeof(range_header)) == ESP_OK) {
        ESP_LOGI(TAG, "Range requested: %s", range_header);
        if (sscanf(range_header, "bytes=%zu-%zu", &start_byte, &end_byte) == 1) {
            end_byte = file_size - 1; // Only start_byte was provided
        }
        if (start_byte >= file_size) {
            httpd_resp_set_status(req, "416 Range Not Satisfiable");
            httpd_resp_send(req, "Requested Range Not Satisfiable", HTTPD_RESP_USE_STRLEN);
            fclose(fd);
            return ESP_FAIL;
        }
        httpd_resp_set_status(req, "206 Partial Content");
        char content_range[128];
        snprintf(content_range, sizeof(content_range), "bytes %zu-%zu/%zu", start_byte, end_byte, file_size);
        httpd_resp_set_hdr(req, "Content-Range", content_range);
        fseek(fd, start_byte, SEEK_SET);
    } else {
        httpd_resp_set_status(req, "200 OK");
    }

    size_t content_length = end_byte - start_byte + 1;

    char *chunk = malloc(4096);
    if (!chunk) {
        ESP_LOGE(TAG, "Failed to allocate memory for download chunk");
        fclose(fd);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    size_t remaining = content_length;
    size_t chunk_size;
    do {
        size_t to_read = (remaining > 4096) ? 4096 : remaining;
        chunk_size = fread(chunk, 1, to_read, fd);
        if (chunk_size > 0) {
            if (httpd_resp_send_chunk(req, chunk, chunk_size) != ESP_OK) {
                fclose(fd);
                free(chunk);
                ESP_LOGE(TAG, "File download chunk sending failed!");
                return ESP_FAIL;
            }
            remaining -= chunk_size;
        }
    } while (chunk_size > 0 && remaining > 0);

    httpd_resp_send_chunk(req, NULL, 0); // End response
    fclose(fd);
    free(chunk);
    ESP_LOGI(TAG, "File download complete: %s", filepath);
    return ESP_OK;
}


static uint32_t cached_sd_total_mb = 0;
static uint32_t cached_sd_used_mb = 0;
static TickType_t last_sd_info_ticks = 0;
static bool sd_info_cached = false;
#define SD_INFO_CACHE_DURATION_MS 10000

static esp_err_t status_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "reader_connected", ebook_reader_connected);
    cJSON_AddBoolToObject(root, "transfer_active", g_transfer_progress.active);
    cJSON_AddBoolToObject(root, "sd_mounted", g_sd_card_initialized);
    cJSON_AddBoolToObject(root, "usb_mounted", g_usb_mounted);

    cJSON_AddBoolToObject(root, "lora_initialized", lora_initialized);
    cJSON_AddBoolToObject(root, "allow_public_uploads", allow_public_uploads);
    cJSON_AddBoolToObject(root, "is_admin", bb_is_admin_request(req));

    // Background Task Status
    cJSON_AddBoolToObject(root, "catalog_updating", catalog_updating);
    #ifdef LORA_USE_SX1262
    extern bool lora_scanning;
    cJSON_AddBoolToObject(root, "lora_scanning", lora_scanning);
    #else
    cJSON_AddBoolToObject(root, "lora_scanning", false);
    #endif



    if (g_sd_card_initialized) {
        TickType_t now_ticks = xTaskGetTickCount();
        // ⚡ Bolt: Removed `|| g_transfer_progress.active` to prevent `esp_vfs_fat_info` from locking VFS during transfers
        if (!sd_info_cached || (now_ticks - last_sd_info_ticks) > pdMS_TO_TICKS(SD_INFO_CACHE_DURATION_MS)) {
            uint64_t out_total_bytes, out_free_bytes;
            if (esp_vfs_fat_info(MOUNT_POINT_SD, &out_total_bytes, &out_free_bytes) == ESP_OK) {
                cached_sd_total_mb = out_total_bytes / (1024 * 1024);
                uint32_t free_mb = out_free_bytes / (1024 * 1024);
                cached_sd_used_mb = cached_sd_total_mb > free_mb ? cached_sd_total_mb - free_mb : 0;
                last_sd_info_ticks = now_ticks;
                sd_info_cached = true;
            }
        }

        if (sd_info_cached) {
            cJSON_AddNumberToObject(root, "sd_total_mb", cached_sd_total_mb);
            cJSON_AddNumberToObject(root, "sd_used_mb", cached_sd_used_mb);
        }
    }

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
    size_t query_len = httpd_req_get_url_query_len(req);
    if (query_len == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }

    char *buf = malloc(query_len + 1);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    if (httpd_req_get_url_query_str(req, buf, query_len + 1) != ESP_OK) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }

    char param[32];
    if (httpd_query_key_value(buf, "type", param, sizeof(param)) != ESP_OK) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }
    free(buf);

    const char *mount_path = (strcmp(param, "sd") == 0) ? MOUNT_POINT_SD : MOUNT_POINT_USB;

    if (strcmp(param, "sd") == 0 && !g_sd_card_initialized) {
         httpd_resp_set_type(req, "application/json");
         httpd_resp_send(req, "[]", 2);
         return ESP_OK;
    }

    if (strcmp(param, "usb") == 0 && !ebook_reader_connected) {
         httpd_resp_set_type(req, "application/json");
         httpd_resp_send(req, "[]", 2);
         return ESP_OK;
    }

    if (strcmp(param, "sd") == 0) {
        char catalog_path[256];
        snprintf(catalog_path, sizeof(catalog_path), "%s/catalog.json", mount_path);
        FILE *f = fopen(catalog_path, "r");
        if (f) {
            httpd_resp_set_type(req, "application/json");
            // ⚡ Bolt: Stream large catalog.json in 4KB chunks instead of loading entirely into RAM with malloc()
            // This prevents OOM crashes on ESP32 by turning O(N) heap usage into O(1), and improves TTFB.
            char *chunk = malloc(4096);
            if (chunk) {
                size_t chunk_size;
                do {
                    chunk_size = fread(chunk, 1, 4096, f);
                    if (chunk_size > 0) {
                        if (httpd_resp_send_chunk(req, chunk, chunk_size) != ESP_OK) {
                            ESP_LOGE(TAG, "Failed to send catalog.json chunk");
                            break;
                        }
                    }
                } while (chunk_size == 4096);
                httpd_resp_send_chunk(req, NULL, 0); // End chunked response
                free(chunk);
            } else {
                httpd_resp_send_500(req);
            }
            fclose(f);
            return ESP_OK;
        }
    }

    DIR *d = opendir(mount_path);
    if (!d) {
        ESP_LOGW(TAG, "Failed to open directory: %s", mount_path);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "[]", 2);
        return ESP_OK;
    }

    cJSON *root = cJSON_CreateArray();
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (dir->d_type == DT_REG) { // If it's a regular file
            const char *ext = strrchr(dir->d_name, '.');
            if (ext && (strcasecmp(ext, ".epub") == 0 || strcasecmp(ext, ".mobi") == 0 ||
                        strcasecmp(ext, ".pdf") == 0 || strcasecmp(ext, ".txt") == 0)) {
                cJSON *file_obj = cJSON_CreateObject();
                cJSON_AddStringToObject(file_obj, "name", dir->d_name);

                // For EPUBs, miniz not available yet, just use filename
                if (strcasecmp(ext, ".epub") == 0) {
                    cJSON_AddStringToObject(file_obj, "title", dir->d_name);
                    cJSON_AddStringToObject(file_obj, "author", "Unknown Author");
                } else {
                    // For other file types, just use the filename
                    cJSON_AddStringToObject(file_obj, "title", dir->d_name);
                    cJSON_AddStringToObject(file_obj, "author", "");
                }
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
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "A file transfer is already in progress.");
        return ESP_FAIL;
    }

    char content[256];
    if (req->content_len >= sizeof(content)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
        return ESP_FAIL;
    }
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        g_led_state = ebook_reader_connected ? LED_STATE_CONNECTED : LED_STATE_IDLE;
        return ESP_FAIL;
    }
    content[ret] = 0;

    cJSON *json = cJSON_Parse(content);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        g_led_state = ebook_reader_connected ? LED_STATE_CONNECTED : LED_STATE_IDLE;
        return ESP_FAIL;
    }

    cJSON *j_source = cJSON_GetObjectItem(json, "source");
    cJSON *j_destination = cJSON_GetObjectItem(json, "destination");
    cJSON *j_filename = cJSON_GetObjectItem(json, "filename");

    if (!j_source || !cJSON_IsString(j_source) ||
        !j_destination || !cJSON_IsString(j_destination) ||
        !j_filename || !cJSON_IsString(j_filename)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        g_led_state = ebook_reader_connected ? LED_STATE_CONNECTED : LED_STATE_IDLE;
        return ESP_FAIL;
    }

    const char *source = j_source->valuestring;
    const char *destination = j_destination->valuestring;
    const char *filename = j_filename->valuestring;

    // Basic path traversal prevention
    if (strstr(filename, "..") != NULL) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Filename");
        g_led_state = ebook_reader_connected ? LED_STATE_CONNECTED : LED_STATE_IDLE;
        return ESP_FAIL;
    }

    // Prevent transferring sensitive files
    if (strcasestr(filename, "adminkey.json") != NULL || strcasestr(filename, ".key") != NULL) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Forbidden");
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

    // Wait for physical confirmation if transferring to USB
    if (strcmp(destination, "sd") != 0) {
        g_waiting_for_transfer_confirm = true;
        g_transfer_confirmed = false;
        g_led_state = LED_STATE_WAIT_CONFIRM;

        ESP_LOGI(TAG, "Waiting for physical button press to confirm transfer to USB...");

        // Wait loop - timeout after 60 seconds
        int timeout = 600; // 600 * 100ms = 60s
        while (g_waiting_for_transfer_confirm && !g_transfer_confirmed && !g_cancel_transfer && timeout > 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            timeout--;
        }

        g_waiting_for_transfer_confirm = false;

        if (g_cancel_transfer || timeout == 0 || !g_transfer_confirmed) {
            ESP_LOGW(TAG, "Transfer cancelled or timed out waiting for confirmation.");
            cJSON_Delete(json);
            g_transfer_progress.active = false;
            g_led_state = ebook_reader_connected ? LED_STATE_CONNECTED : LED_STATE_IDLE;

            cJSON *response_json = cJSON_CreateObject();
            cJSON_AddBoolToObject(response_json, "success", false);
            cJSON_AddStringToObject(response_json, "message", "Transfer cancelled or timed out waiting for button press.");
            char *json_str = cJSON_PrintUnformatted(response_json);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, json_str, strlen(json_str));
            free(json_str);
            cJSON_Delete(response_json);
            return ESP_OK;
        }
    }

    g_led_state = LED_STATE_TRANSFER;

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
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
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

static esp_err_t sleep_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Received request to enter deep sleep.");
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

    // Short delay to ensure the HTTP response is sent before sleeping
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Unmounting filesystems before sleep...");
    // Unmount SPIFFS. The label "storage" is defined in init_spiffs.
    if (esp_vfs_spiffs_unregister("storage") == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS unmounted successfully.");
    } else {
        ESP_LOGE(TAG, "Failed to unmount SPIFFS.");
    }
    // Unmount SD card
    if (esp_vfs_fat_sdcard_unmount(MOUNT_POINT_SD, NULL) == ESP_OK) {
         ESP_LOGI(TAG, "SD card unmounted successfully.");
    } else {
         ESP_LOGE(TAG, "Failed to unmount SD card.");
    }

    // Turn off LEDs
    led_strip_clear(g_led_strip);

    esp_deep_sleep_start();
    // This function does not return
    return ESP_OK;
}


static esp_err_t set_led_color_handler(httpd_req_t *req) {
    if (!bb_is_admin_request(req)) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Forbidden");
        return ESP_FAIL;
    }

    char content[100];
    if (req->content_len >= sizeof(content)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
        return ESP_FAIL;
    }
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        return ESP_FAIL;
    }
    content[ret] = 0;

    cJSON *json = cJSON_Parse(content);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *r_item = cJSON_GetObjectItem(json, "r");
    cJSON *g_item = cJSON_GetObjectItem(json, "g");
    cJSON *b_item = cJSON_GetObjectItem(json, "b");

    if (r_item && cJSON_IsNumber(r_item) && g_item && cJSON_IsNumber(g_item) && b_item && cJSON_IsNumber(b_item)) {
        g_led_color_r = r_item->valueint;
        g_led_color_g = g_item->valueint;
        g_led_color_b = b_item->valueint;
        save_led_color_to_nvs(g_led_color_r, g_led_color_g, g_led_color_b);
        httpd_resp_send(req, "{\"success\":true}", 16);
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing RGB values");
    }

    cJSON_Delete(json);
    return ESP_OK;
}

// --- WEB SERVER SETUP (MAIN APP) ---
static esp_err_t admin_set_public_uploads_handler(httpd_req_t *req) {
    if (!bb_is_admin_request(req)) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Forbidden");
        return ESP_FAIL;
    }

    char content[100];
    if (req->content_len >= sizeof(content)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
        return ESP_FAIL;
    }
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        return ESP_FAIL;
    }
    content[ret] = 0;

    cJSON *json = cJSON_Parse(content);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *enabled_item = cJSON_GetObjectItem(json, "enabled");
    if (!cJSON_IsBool(enabled_item)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    allow_public_uploads = cJSON_IsTrue(enabled_item);
    cJSON_Delete(json);

    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        nvs_set_u8(my_handle, NVS_KEY_PUBLIC_UPLOADS, allow_public_uploads ? 1 : 0);
        nvs_commit(my_handle);
        nvs_close(my_handle);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\": true}", 17);
    return ESP_OK;
}

httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 32;
    config.stack_size = 8192;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Handlers for dynamic content
        httpd_uri_t status_uri = { "/status", HTTP_GET, status_handler, NULL };
        httpd_register_uri_handler(server, &status_uri);

        httpd_uri_t list_uri = { "/list-files", HTTP_GET, list_files_handler, NULL };
        httpd_register_uri_handler(server, &list_uri);

        httpd_uri_t transfer_uri = { "/transfer-file", HTTP_POST, transfer_file_handler, NULL };
        httpd_register_uri_handler(server, &transfer_uri);

        httpd_uri_t delete_file_uri = { "/delete-file", HTTP_POST, delete_file_handler, NULL };
        httpd_register_uri_handler(server, &delete_file_uri);


        httpd_uri_t progress_uri = { "/transfer-progress", HTTP_GET, transfer_progress_handler, NULL };
        httpd_register_uri_handler(server, &progress_uri);

        httpd_uri_t cancel_uri = { "/transfer-cancel", HTTP_POST, transfer_cancel_handler, NULL };
        httpd_register_uri_handler(server, &cancel_uri);

        httpd_uri_t sleep_uri = { "/enter-sleep", HTTP_POST, sleep_handler, NULL };
        httpd_register_uri_handler(server, &sleep_uri);
        httpd_uri_t download_uri = { "/download", HTTP_GET, download_handler, NULL };
        httpd_register_uri_handler(server, &download_uri);

        httpd_uri_t upload_uri = { "/upload", HTTP_POST, upload_handler, NULL };
        httpd_register_uri_handler(server, &upload_uri);


        httpd_uri_t public_uploads_uri = { "/admin/set_public_uploads", HTTP_POST, admin_set_public_uploads_handler, NULL };
        httpd_register_uri_handler(server, &public_uploads_uri);


        httpd_uri_t setup_uri = { "/setup", HTTP_GET, setup_get_handler, NULL };
        httpd_register_uri_handler(server, &setup_uri);

        httpd_uri_t save_uri = { "/save-credentials", HTTP_POST, save_credentials_post_handler, NULL };
        httpd_register_uri_handler(server, &save_uri);

        httpd_uri_t set_led_uri = { "/set-led-color", HTTP_POST, set_led_color_handler, NULL };
        httpd_register_uri_handler(server, &set_led_uri);

        // Register bulletin board API endpoints
        register_bulletin_api_handlers(server);
        register_audio_api_handlers(server);

        // Handler for all other URIs (serves static files)
        httpd_uri_t static_uri = { "/*", HTTP_GET, static_file_handler, NULL };
        httpd_register_uri_handler(server, &static_uri);
    }
    return server;
}


// --- BLE Definitions ---
#define GATTS_TAG "WIFI_PROV_GATTS"
#define BLE_DEVICE_NAME "E-Book Librarian Setup"

#define WIFI_PROV_PROFILE_NUM           1
#define WIFI_PROV_PROFILE_APP_ID        0
#define SVC_INST_ID                     0

/* Service */
static const uint16_t GATTS_SERVICE_UUID_WIFI_PROV   = 0x180A; // Using Device Information Service UUID for simplicity
static const uint16_t GATTS_CHAR_UUID_SSID           = 0x2A24; // Model Number String
static const uint16_t GATTS_CHAR_UUID_PASS           = 0x2A25; // Serial Number String
static const uint16_t GATTS_CHAR_UUID_SAVE           = 0x2A26; // Firmware Revision String
static const uint16_t GATTS_CHAR_UUID_STATUS         = 0x2A29; // Manufacturer Name String

static const uint16_t primary_service_uuid         = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid   = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

static const uint8_t char_prop_write               = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_read_notify         = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

/// Wifi Provisioning Service - Attribute Table
enum
{
    IDX_SVC,

    IDX_CHAR_SSID,
    IDX_CHAR_VAL_SSID,

    IDX_CHAR_PASS,
    IDX_CHAR_VAL_PASS,

    IDX_CHAR_SAVE,
    IDX_CHAR_VAL_SAVE,

    IDX_CHAR_STATUS,
    IDX_CHAR_VAL_STATUS,
    IDX_CHAR_CFG_STATUS,

    WIFI_PROV_IDX_NB,
};

static const esp_gatts_attr_db_t gatt_db[WIFI_PROV_IDX_NB] =
{
    // Service Declaration
    [IDX_SVC]        =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
      sizeof(uint16_t), sizeof(GATTS_SERVICE_UUID_WIFI_PROV), (uint8_t *)&GATTS_SERVICE_UUID_WIFI_PROV}},

    /* SSID Characteristic Declaration */
    [IDX_CHAR_SSID]     =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_write}},

    /* SSID Characteristic Value */
    [IDX_CHAR_VAL_SSID] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_SSID, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      64, 0, NULL}},

    /* Password Characteristic Declaration */
    [IDX_CHAR_PASS]      =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_write}},

    /* Password Characteristic Value */
    [IDX_CHAR_VAL_PASS]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_PASS, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      64, 0, NULL}},

    /* Save Characteristic Declaration */
    [IDX_CHAR_SAVE]      =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_write}},

    /* Save Characteristic Value */
    [IDX_CHAR_VAL_SAVE]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_SAVE, ESP_GATT_PERM_WRITE,
      sizeof(uint8_t), 0, NULL}},

    /* Status Characteristic Declaration */
    [IDX_CHAR_STATUS]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_read_notify}},

    /* Status Characteristic Value */
    [IDX_CHAR_VAL_STATUS] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_STATUS, ESP_GATT_PERM_READ,
      32, 0, NULL}},

    /* Status Client Characteristic Configuration Descriptor */
    [IDX_CHAR_CFG_STATUS]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      sizeof(uint16_t), 0, NULL}},
};

static uint16_t gatt_db_handle_table[WIFI_PROV_IDX_NB];
static char wifi_ssid[33] = {0};
static char wifi_password[65] = {0};

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
};

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst wifi_prov_profile_tab[WIFI_PROV_PROFILE_NUM] = {
    [WIFI_PROV_PROFILE_APP_ID] = {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};


// --- BLE Advertising Data ---
static uint8_t adv_config_done = 0;
#define adv_config_flag      (1 << 0)
#define scan_rsp_config_flag (1 << 1)

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval        = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance          = 0x00,
    .manufacturer_len    = 0,
    .p_manufacturer_data = NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = sizeof(GATTS_SERVICE_UUID_WIFI_PROV),
    .p_service_uuid      = (uint8_t*)&GATTS_SERVICE_UUID_WIFI_PROV,
    .flag                = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};


// --- BLE Event Handlers ---
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~adv_config_flag);
        if (adv_config_done == 0){
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(GATTS_TAG, "Advertising start failed");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(GATTS_TAG, "Advertising stop failed");
        } else {
            ESP_LOGI(GATTS_TAG, "Stop adv successfully");
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
         ESP_LOGI(GATTS_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            wifi_prov_profile_tab[param->reg.app_id].gatts_if = gatts_if;
        } else {
            ESP_LOGI(GATTS_TAG, "Reg app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
            return;
        }
    }

    do {
        int idx;
        for (idx = 0; idx < WIFI_PROV_PROFILE_NUM; idx++) {
            if (gatts_if == ESP_GATT_IF_NONE || gatts_if == wifi_prov_profile_tab[idx].gatts_if) {
                if (wifi_prov_profile_tab[idx].gatts_cb) {
                    wifi_prov_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}


// --- BLE Initialization ---
void init_ble(void)
{
    esp_err_t ret;

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(GATTS_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(GATTS_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret){
        ESP_LOGE(GATTS_TAG, "gatts register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret){
        ESP_LOGE(GATTS_TAG, "gap register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gatts_app_register(WIFI_PROV_PROFILE_APP_ID);
    if (ret){
        ESP_LOGE(GATTS_TAG, "gatts app register error, error code = %x", ret);
        return;
    }

    ESP_LOGI(GATTS_TAG, "BLE Initialized successfully");
}


static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            ESP_LOGI(GATTS_TAG, "REGISTER_APP_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
            esp_ble_gap_set_device_name(BLE_DEVICE_NAME);
            esp_ble_gap_config_adv_data(&adv_data);
            adv_config_done |= adv_config_flag;

            esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, WIFI_PROV_IDX_NB, SVC_INST_ID);
            break;
        case ESP_GATTS_READ_EVT: {
            ESP_LOGI(GATTS_TAG, "GATT_READ_EVT, conn_id %d, trans_id %lu, handle %d", param->read.conn_id, (unsigned long)param->read.trans_id, param->read.handle);
            esp_gatt_rsp_t rsp;
            memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
            rsp.attr_value.handle = param->read.handle;
            rsp.attr_value.len = 4;
            rsp.attr_value.value[0] = 0xde;
            rsp.attr_value.value[1] = 0xad;
            rsp.attr_value.value[2] = 0xbe;
            rsp.attr_value.value[3] = 0xef;
            esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
            break;
        }
        case ESP_GATTS_WRITE_EVT: {
            ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, conn_id %d, trans_id %lu, handle %d", param->write.conn_id, (unsigned long)param->write.trans_id, param->write.handle);

            if (param->write.handle == gatt_db_handle_table[IDX_CHAR_VAL_SSID]) {
                size_t len = param->write.len < sizeof(wifi_ssid) - 1 ? param->write.len : sizeof(wifi_ssid) - 1;
                memcpy(wifi_ssid, param->write.value, len);
                wifi_ssid[len] = '\0';
                ESP_LOGI(GATTS_TAG, "SSID set to: %s", wifi_ssid);
            } else if (param->write.handle == gatt_db_handle_table[IDX_CHAR_VAL_PASS]) {
                size_t len = param->write.len < sizeof(wifi_password) - 1 ? param->write.len : sizeof(wifi_password) - 1;
                memcpy(wifi_password, param->write.value, len);
                wifi_password[len] = '\0';
                ESP_LOGI(GATTS_TAG, "Password set."); // Don't log the password
            } else if (param->write.handle == gatt_db_handle_table[IDX_CHAR_VAL_SAVE]) {
                if (param->write.len == 1 && param->write.value[0] == 1) {
                    ESP_LOGI(GATTS_TAG, "Save command received. Saving credentials and restarting.");
                    save_wifi_credentials(wifi_ssid, wifi_password);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                }
            }

            if (param->write.need_rsp){
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            break;
        }
        case ESP_GATTS_EXEC_WRITE_EVT:
            break;
        case ESP_GATTS_MTU_EVT:
            break;
        case ESP_GATTS_CONF_EVT:
            break;
        case ESP_GATTS_START_EVT:
            break;
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(GATTS_TAG, "CONNECT_EVT, conn_id %d", param->connect.conn_id);
            wifi_prov_profile_tab[WIFI_PROV_PROFILE_APP_ID].conn_id = param->connect.conn_id;
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(GATTS_TAG, "DISCONNECT_EVT, reason 0x%x", param->disconnect.reason);
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:{
            if (param->add_attr_tab.status != ESP_GATT_OK){
                ESP_LOGE(GATTS_TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
            }
            else if (param->add_attr_tab.num_handle != WIFI_PROV_IDX_NB){
                ESP_LOGE(GATTS_TAG, "create attribute table abnormally, num_handle (%d) doesn't equal to WIFI_PROV_IDX_NB(%d)", param->add_attr_tab.num_handle, WIFI_PROV_IDX_NB);
            }
            else {
                ESP_LOGI(GATTS_TAG, "create attribute table successfully, the number handle = %d",param->add_attr_tab.num_handle);
                memcpy(gatt_db_handle_table, param->add_attr_tab.handles, sizeof(gatt_db_handle_table));
                esp_ble_gatts_start_service(gatt_db_handle_table[IDX_SVC]);
            }
            break;
        }
        default:
            break;
    }
}


// --- WI-FI SETUP ---
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        (void)event;
        (void)event;
        ESP_LOGI(TAG, "station %02x:%02x:%02x:%02x:%02x:%02x join, AID=%d", event->mac[0], event->mac[1], event->mac[2], event->mac[3], event->mac[4], event->mac[5], event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        (void)event;
        (void)event;
        ESP_LOGI(TAG, "station %02x:%02x:%02x:%02x:%02x:%02x leave, AID=%d", event->mac[0], event->mac[1], event->mac[2], event->mac[3], event->mac[4], event->mac[5], event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Attempting to connect to the AP...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Failed to connect to the AP.");
        xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void init_wifi(void) {
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    char ssid[32];
    char password[64];
    if (load_wifi_credentials(ssid, sizeof(ssid), password, sizeof(password)) == ESP_OK) {
        ESP_LOGI(TAG, "Credentials found. Connecting to '%s'", ssid);
        esp_netif_create_default_wifi_sta();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

        wifi_config_t wifi_config = { .sta = { .threshold.authmode = WIFI_AUTH_WPA_PSK } };
        strlcpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
        strlcpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Connected to AP successfully!");
            g_wifi_configured = true;
        } else {
            ESP_LOGW(TAG, "Failed to connect. Will start AP for configuration.");
            g_wifi_configured = false;
            ESP_ERROR_CHECK(esp_wifi_stop());
            ESP_ERROR_CHECK(esp_wifi_deinit());
        }
    }

    if (!g_wifi_configured) {
        ESP_LOGI(TAG, "Starting in AP mode for configuration.");
        esp_netif_create_default_wifi_ap();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

        wifi_config_t wifi_config = {
            .ap = {
                .ssid = WIFI_AP_SSID,
                .password = WIFI_AP_PASS,
                .ssid_len = strlen(WIFI_AP_SSID),
                .channel = 1,
                .authmode = WIFI_AUTH_OPEN,
                .max_connection = WIFI_AP_MAX_STA_CONN,
            },
        };
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI(TAG, "WiFi AP initialized for setup. SSID:%s", WIFI_AP_SSID);
    }
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
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    const char mount_point[] = MOUNT_POINT_SD;
    ESP_LOGI(TAG, "Initializing SD card");

    // Enable internal pull-ups on SD card pins
    gpio_set_pull_mode(PIN_NUM_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_NUM_MISO, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_NUM_CLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_NUM_CS, GPIO_PULLUP_ONLY);

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .max_transfer_sz = 4000,
    };
    // Ensure SPI3 is used for SD card to avoid conflict with LoRa on SPI2
    host.slot = SPI3_HOST;

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize spi bus for SD card (soft fail).");
        g_sd_card_initialized = false;
        return;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &g_sd_card_handle);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to mount SD card VFS (soft fail)");
        g_sd_card_initialized = false;
    } else {
        ESP_LOGI(TAG, "SD card mounted successfully at %s", mount_point);
        g_sd_card_initialized = true;
    }
}

bool format_sd_card(void) {
    if (!g_sd_card_initialized || g_sd_card_handle == NULL) {
        ESP_LOGW(TAG, "Cannot format SD card: Not initialized or not mounted.");
        return false;
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    ESP_LOGI(TAG, "Explicitly formatting SD card...");
    esp_err_t err = esp_vfs_fat_sdcard_format_cfg(MOUNT_POINT_SD, g_sd_card_handle, &mount_config);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "SD Card formatted successfully!");
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to format SD card: %s", esp_err_to_name(err));
        return false;
    }
}

// --- Calibre DB Import ---
void import_from_calibre_db(const char* usb_mount_path) {
    char db_path[256];
    snprintf(db_path, sizeof(db_path), "%s/metadata.db", usb_mount_path);

    struct stat st;
    if (stat(db_path, &st) != 0) {
        ESP_LOGI(TAG, "Calibre metadata.db not found at %s. Skipping import.", db_path);
        return;
    }

    ESP_LOGI(TAG, "Found Calibre database at %s. Attempting to import.", db_path);

    sqlite3 *db;
    int rc = sqlite3_open(db_path, &db);
    if (rc) {
        ESP_LOGE(TAG, "Can't open database: %s", sqlite3_errmsg(db));
        return;
    } else {
        ESP_LOGI(TAG, "Opened database successfully");
    }

    sqlite3_stmt *res;
    const char *sql = "SELECT b.title, a.name as author, b.path, d.name as filename, d.format "
                      "FROM books b "
                      "LEFT JOIN books_authors_link bal ON b.id = bal.book "
                      "LEFT JOIN authors a ON bal.author = a.id "
                      "LEFT JOIN data d ON b.id = d.book "
                      "WHERE d.format IN ('EPUB', 'MOBI', 'PDF', 'TXT') "
                      "ORDER BY b.title";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
    if (rc != SQLITE_OK) {
        ESP_LOGE(TAG, "Failed to execute statement: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    ESP_LOGI(TAG, "--- Calibre Book Import ---");
    while (sqlite3_step(res) == SQLITE_ROW) {
        const unsigned char *title = sqlite3_column_text(res, 0);
        const unsigned char *author = sqlite3_column_text(res, 1);
        const unsigned char *path = sqlite3_column_text(res, 2);
        const unsigned char *filename = sqlite3_column_text(res, 3);
        const unsigned char *format = sqlite3_column_text(res, 4);

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s/%s", usb_mount_path, path, filename);

        ESP_LOGI(TAG, "Title: %s, Author: %s, Format: %s, Path: %s",
            title ? (char*)title : "N/A",
            author ? (char*)author : "N/A",
            format ? (char*)format : "N/A",
            full_path
        );
    }
    ESP_LOGI(TAG, "--- End of Import ---");

    sqlite3_finalize(res);
    sqlite3_close(db);
}/**
 * @brief Set configuration callback
 *
 * Set the USB device configuration during the enumeration process, must be enabled in the menuconfig

 * @note bConfigurationValue starts at index 1
 *
 * @param[in] dev_desc device descriptor of the USB device currently being enumerated
 * @param[out] bConfigurationValue configuration descriptor index, that will be user for enumeration
 *
 * @return bool
 * - true:  USB device will be enumerated
 * - false: USB device will not be enumerated
 */
#ifdef ENABLE_ENUM_FILTER_CALLBACK
static bool set_config_cb(const usb_device_desc_t *dev_desc, uint8_t *bConfigurationValue)
{
    // If the USB device has more than one configuration, set the second configuration
    if (dev_desc->bNumConfigurations > 1) {
        *bConfigurationValue = 2;
    } else {
        *bConfigurationValue = 1;
    }

    // Return true to enumerate the USB device
    return true;
}
#endif // ENABLE_ENUM_FILTER_CALLBACK
/**
 * @brief Start USB Host install and handle common USB host library events while app pin not low
 *
 * @param[in] arg  Not used
 */
static void usb_host_lib_task(void *arg)
{
    ESP_LOGI(TAG, "Installing USB Host Library");
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LOWMED,
# ifdef ENABLE_ENUM_FILTER_CALLBACK
        .enum_filter_cb = set_config_cb,
# endif // ENABLE_ENUM_FILTER_CALLBACK
        // // .peripheral_map = BIT0,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));
    // // ESP_LOGI(TAG, "USB Host installed with peripheral map 0x%x", host_config.peripheral_map);

    //Signalize the app_main, the USB host library has been installed
    xTaskNotifyGive(arg);

    bool has_clients = true;
    bool has_devices = false;
    while (has_clients) {
        uint32_t event_flags;
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &event_flags));
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGI(TAG, "Get FLAGS_NO_CLIENTS");
            if (ESP_OK == usb_host_device_free_all()) {
                ESP_LOGI(TAG, "All devices marked as free, no need to wait FLAGS_ALL_FREE event");
                has_clients = false;
            } else {
                ESP_LOGI(TAG, "Wait for the FLAGS_ALL_FREE");
                has_devices = true;
            }
        }
        if (has_devices && event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "Get FLAGS_ALL_FREE");
            has_clients = false;
        }
    }
    ESP_LOGI(TAG, "No more clients and devices, uninstall USB Host library");

    //Uninstall the USB Host Library
    ESP_ERROR_CHECK(usb_host_uninstall());
    vTaskSuspend(NULL);
}


// --- USB HOST SETUP ---
static void msc_event_cb(const msc_host_event_t *event, void *arg)
{
    if (event->event == MSC_DEVICE_CONNECTED) {
        ESP_LOGI(TAG, "MSC device connected");
        ebook_reader_connected = true;
        g_led_state = LED_STATE_CONNECTED;
        ESP_ERROR_CHECK(msc_host_install_device(event->device.address, &device_handle));
        esp_vfs_fat_mount_config_t msc_mount_config = {
            .format_if_mount_failed = false,
            .max_files = 5,
            .allocation_unit_size = 16 * 1024
        };

        // Mount the filesystem
        if (msc_host_vfs_register(device_handle, MOUNT_POINT_USB, &msc_mount_config, &vfs_handle) == ESP_OK) {
            ESP_LOGI(TAG, "MSC device mounted at %s", MOUNT_POINT_USB);

            // Check for hardware auth key file
            char key_file_path[128];
            snprintf(key_file_path, sizeof(key_file_path), "%s/.library_admin.key", MOUNT_POINT_USB);
            FILE *kf = fopen(key_file_path, "r");
            if (kf) {
                char key_buf[65] = {0};
                size_t read_bytes = fread(key_buf, 1, 64, kf);
                fclose(kf);
                if (read_bytes > 0) {
                    // trim trailing whitespace
                    while (read_bytes > 0 && isspace((unsigned char)key_buf[read_bytes-1])) {
                        key_buf[read_bytes-1] = '\0';
                        read_bytes--;
                    }
                    if (bb_check_key(key_buf)) {
                        g_hardware_key_authenticated = true;
                        ESP_LOGI(TAG, "Hardware admin key authenticated via USB");
                    }
                }
            }

            // Attempt to import from Calibre DB
            import_from_calibre_db(MOUNT_POINT_USB);
        } else {
            ESP_LOGE(TAG, "Failed to mount MSC device");
            g_led_state = LED_STATE_ERROR;
        }

    } else if (event->event == MSC_DEVICE_DISCONNECTED) {
        ESP_LOGI(TAG, "MSC device disconnected");
        ebook_reader_connected = false;
        g_hardware_key_authenticated = false;
        g_led_state = LED_STATE_IDLE;
        // Unmount the filesystem
        msc_host_vfs_unregister(vfs_handle);
        ESP_LOGI(TAG, "MSC device unmounted");
        msc_host_uninstall_device(device_handle);
    }
}

void usb_host_lib_task_dummy(void *arg)
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


    ESP_LOGI(TAG, "Installing MSC client");
    const msc_host_driver_config_t msc_config = {
        .create_backround_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .callback = msc_event_cb,
    };
    esp_err_t err = msc_host_install(&msc_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install MSC client");
        g_usb_mounted = false;
    } else {
        g_usb_mounted = true;
    }
}

// --- LED STRIP ---
static void led_status_task(void *pvParameters) {
    uint8_t brightness = 0;
    bool increasing = true;

    while (1) {
        switch (g_led_state) {
            case LED_STATE_IDLE: // Slow breathing custom color
                if (increasing) {
                    brightness += 1;
                    if (brightness >= 80) increasing = false;
                } else {
                    brightness -= 1;
                    if (brightness <= 5) increasing = true;
                }
                for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
                    // Set all LEDs to custom color with the current brightness scaling
                    uint32_t r = (g_led_color_r * brightness) / 100;
                    uint32_t g = (g_led_color_g * brightness) / 100;
                    uint32_t b = (g_led_color_b * brightness) / 100;
                    led_strip_set_pixel(g_led_strip, i, r, g, b);
                }
                led_strip_refresh(g_led_strip);
                vTaskDelay(pdMS_TO_TICKS(35));
                break;

            case LED_STATE_SETUP: // Pulsing purple
                if (increasing) {
                    brightness += 2;
                    if (brightness >= 100) increasing = false;
                } else {
                    brightness -= 2;
                    if (brightness <= 0) increasing = true;
                }
                for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
                    led_strip_set_pixel(g_led_strip, i, brightness, 0, brightness);
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

            case LED_STATE_EJECT: // Quick Green Blink
                for (int j=0; j<2; j++) {
                    for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
                        led_strip_set_pixel(g_led_strip, i, 0, 255, 0); // Green
                    }
                    led_strip_refresh(g_led_strip);
                    vTaskDelay(pdMS_TO_TICKS(150));
                    led_strip_clear(g_led_strip);
                    vTaskDelay(pdMS_TO_TICKS(150));
                }
                g_led_state = LED_STATE_IDLE; // Revert to idle state
                break;

            case LED_STATE_WAIT_CONFIRM: // Fast blinking yellow
                for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
                    led_strip_set_pixel(g_led_strip, i, 200, 150, 0); // Yellow/Orange
                }
                led_strip_refresh(g_led_strip);
                vTaskDelay(pdMS_TO_TICKS(200));
                led_strip_clear(g_led_strip);
                vTaskDelay(pdMS_TO_TICKS(200));
                break;

            default:
                 vTaskDelay(pdMS_TO_TICKS(100));
                 break;
        }
    }
}

void init_led_strip(void) {
    ESP_LOGI(TAG, "Install WS2812 driver");
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .clk_src = RMT_CLK_SRC_DEFAULT,
    };
    led_strip_config_t strip_config = {
        .max_leds = LED_STRIP_LED_NUMBERS,
        .strip_gpio_num = LED_STRIP_GPIO,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &g_led_strip));

    led_strip_clear(g_led_strip);
}

/**
 * @brief BOOT button pressed callback
 *
 * Signal application to exit the Host lib task
 *
 * @param[in] arg Unused
 */
static void gpio_cb(void *arg)
{
    const app_event_queue_t evt_queue = {
        .event_group = APP_EVENT,
    };

    BaseType_t xTaskWoken = pdFALSE;

    if (app_event_queue) {
        xQueueSendFromISR(app_event_queue, &evt_queue, &xTaskWoken);
    }

    if (xTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}
// --- Eject/Sleep Button Task ---
void eject_button_task(void *pvParameters) {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << EJECT_BUTTON_GPIO);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "Eject/Sleep button task started on GPIO %d", EJECT_BUTTON_GPIO);

    while (1) {
        if (gpio_get_level(EJECT_BUTTON_GPIO) == 0) {
            // Button is pressed, start checking for long press
            vTaskDelay(pdMS_TO_TICKS(50)); // Debounce
            int long_press_counter = 0;
            while (gpio_get_level(EJECT_BUTTON_GPIO) == 0) {
                long_press_counter++;
                vTaskDelay(pdMS_TO_TICKS(50));
                if (long_press_counter > 40) { // 40 * 50ms = 2 seconds
                    ESP_LOGI(TAG, "Long press detected. Entering deep sleep.");

                    ESP_LOGI(TAG, "Unmounting filesystems before sleep...");
                    // Unmount SPIFFS. The label "storage" is defined in init_spiffs.
                    if (esp_vfs_spiffs_unregister("storage") == ESP_OK) {
                        ESP_LOGI(TAG, "SPIFFS unmounted successfully.");
                    } else {
                        ESP_LOGE(TAG, "Failed to unmount SPIFFS.");
                    }
                    // Unmount SD card
                    if (esp_vfs_fat_sdcard_unmount(MOUNT_POINT_SD, NULL) == ESP_OK) {
                         ESP_LOGI(TAG, "SD card unmounted successfully.");
                    } else {
                         ESP_LOGE(TAG, "Failed to unmount SD card.");
                    }

                    // Turn off LEDs before sleeping
                    led_strip_clear(g_led_strip);
                    esp_deep_sleep_start();
                    // This function does not return
                }
            }

            // If we get here, it was a short press
            ESP_LOGI(TAG, "Short press detected.");

            if (g_waiting_for_transfer_confirm) {
                ESP_LOGI(TAG, "Transfer confirmed by physical button press.");
                g_transfer_confirmed = true;
                g_waiting_for_transfer_confirm = false;
                // Don't unmount or show eject LED if we are just confirming a transfer
            } else if (ebook_reader_connected) {
                ESP_LOGI(TAG, "Unmounting USB drive...");
                msc_host_vfs_unregister(vfs_handle);
                // The msc_event_cb will set ebook_reader_connected to false
                // and the LED state to IDLE. We will override it here for feedback.
                g_led_state = LED_STATE_EJECT;
            } else {
                ESP_LOGW(TAG, "Eject button pressed, but no USB drive connected.");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // Poll every 20ms
    }
}


// --- MAIN APPLICATION ENTRY POINT ---
void app_main(void) {
    ESP_LOGI(TAG, "USB host library lamp");

    // Init BOOT button: Pressing the button simulates app request to exit
    // It will uninstall the class driver and USB Host Lib
    const gpio_config_t input_pin = {
        .pin_bit_mask = BIT64(APP_QUIT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&input_pin));
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LOWMED));
    ESP_ERROR_CHECK(gpio_isr_handler_add(APP_QUIT_PIN, gpio_cb, NULL));
 
    // Initialize LoRaWAN
    lora_wan_init();

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    load_led_color_from_nvs();


    // Initialize Catalog Update Queue and Task
    catalog_update_queue = xQueueCreate(10, sizeof(catalog_update_msg_t));
    if (catalog_update_queue != NULL) {
        xTaskCreatePinnedToCore(catalog_update_task, "catalog_update_task", 4096, NULL, 5, NULL, 1);
    }

    app_event_queue = xQueueCreate(10, sizeof(app_event_queue_t));
    app_event_queue_t evt_queue;
    (void)evt_queue;
    (void)evt_queue;

    TaskHandle_t host_lib_task_hdl;
    // TaskHandle_t class_driver_task_hdl;

    // Create usb host lib task
    BaseType_t task_created;
    task_created = xTaskCreatePinnedToCore(usb_host_lib_task,
                                           "usb_host",
                                           4096,
                                           xTaskGetCurrentTaskHandle(),
                                           HOST_LIB_TASK_PRIORITY,
                                           &host_lib_task_hdl,
                                           0);
    assert(task_created == pdTRUE);

    // Wait until the USB host library is installed
    ulTaskNotifyTake(false, 1000);

    // Create class driver task

    assert(task_created == pdTRUE);
    // Add a short delay to let the tasks run
    vTaskDelay(10);
    // Create WebSocket Mutex
    ws_mutex = xSemaphoreCreateMutex();

    // Initialize LED strip early so we can show status
    init_led_strip();
    xTaskCreate(led_status_task, "led_status_task", 2048, NULL, 5, NULL);

    // Show setup (purple pulsing) while trying to initialize/connect
    g_led_state = LED_STATE_SETUP;

    // Initialize BLE for configuration
    init_ble();

    init_wifi();

    ESP_LOGI(TAG, "Starting main application and services...");
    init_spiffs();
    init_sd_card();
    init_usb_host();
    start_webserver();

    if (g_wifi_configured) {
        // Normal operation
        ESP_LOGI(TAG, "E-Book Librarian is running in STA mode!");
        g_led_state = LED_STATE_IDLE;
    } else {
        // Configuration mode
        ESP_LOGI(TAG, "Starting AP mode services...");
        g_led_state = LED_STATE_IDLE; // Use standard idle mode in AP mode
        start_dns_server();
        ESP_LOGI(TAG, "AP mode and Captive Portal services are running. Connect to the Wi-Fi AP to configure or browse.");
    }

    // Start the eject button monitoring task
    xTaskCreate(eject_button_task, "eject_button_task", 2048, NULL, 10, NULL);
}
