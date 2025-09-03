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
 * NOTE: This file contains the core application logic. To compile this, you will
 * need to set up a standard ESP-IDF project, which includes a CMakeLists.txt
 * file and component configurations (e.g., via `idf.py menuconfig`).
 * =================================================================================
 */

// --- Standard and ESP-IDF Dependencies ---
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "cJSON.h"

// --- USB Host Dependencies (from the official ESP-IDF stack) ---
#include "esp_usb.h"
#include "usb/usb_host.h"
#include "usb/msc_host.h"
#include "usb/vfs_msc.h"


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

// --- GLOBALS ---
static const char *TAG = "EBOOK_LIBRARIAN";
static bool ebook_reader_connected = false;
static msc_host_device_handle_t device_handle = NULL;


// --- WEB PAGE (HTML, CSS, JS) ---
// We embed the user interface directly into the code.
const char index_html_start[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>E-Book Librarian</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; margin: 0; background-color: #f0f2f5; color: #1c1e21; }
        .container { max-width: 900px; margin: 20px auto; padding: 20px; background-color: #fff; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        h1 { color: #1877f2; text-align: center; border-bottom: 1px solid #ddd; padding-bottom: 10px; }
        .status { padding: 10px; background-color: #e7f3ff; border: 1px solid #b7d9f7; border-radius: 6px; margin-bottom: 20px; text-align: center; }
        .status.connected { background-color: #e9f7ef; border-color: #b7e4c7; }
        .status.disconnected { background-color: #fff1f0; border-color: #ffccc7; }
        .panels { display: flex; flex-wrap: wrap; gap: 20px; }
        .panel { flex: 1; min-width: 300px; background: #f9f9f9; padding: 15px; border-radius: 6px; border: 1px solid #e0e0e0; }
        .panel h2 { margin-top: 0; font-size: 1.2em; }
        ul { list-style-type: none; padding: 0; max-height: 400px; overflow-y: auto; }
        li { padding: 8px 10px; border-bottom: 1px solid #eee; display: flex; align-items: center; justify-content: space-between; word-break: break-all; }
        li:last-child { border-bottom: none; }
        .file-name { cursor: pointer; }
        .file-name.selected { font-weight: bold; color: #1877f2; }
        button { background-color: #1877f2; color: white; border: none; padding: 10px 15px; border-radius: 6px; cursor: pointer; font-size: 1em; transition: background-color 0.2s; }
        button:disabled { background-color: #a0c3f7; cursor: not-allowed; }
        .actions { margin-top: 20px; text-align: center; }
    </style>
</head>
<body>
    <div class="container">
        <h1>📖 E-Book Librarian</h1>
        <div id="status" class="status disconnected">Connecting... Please plug in your e-reader.</div>
        <div class="panels">
            <div class="panel">
                <h2>Library Books (SD Card)</h2>
                <ul id="sd-file-list"></ul>
            </div>
            <div class="panel">
                <h2>E-Reader Books (USB)</h2>
                <ul id="usb-file-list"></ul>
            </div>
        </div>
        <div class="actions">
            <button id="to-reader-btn" disabled>Copy to E-Reader &gt;&gt;</button>
            <button id="to-library-btn" disabled>&lt;&lt; Copy to Library</button>
        </div>
    </div>
    <script>
        let selectedSdFile = null;
        let selectedUsbFile = null;

        const sdList = document.getElementById('sd-file-list');
        const usbList = document.getElementById('usb-file-list');
        const toReaderBtn = document.getElementById('to-reader-btn');
        const toLibraryBtn = document.getElementById('to-library-btn');
        const statusDiv = document.getElementById('status');

        function selectFile(listElement, fileName, type) {
            Array.from(document.querySelectorAll('.file-name.selected')).forEach(el => el.classList.remove('selected'));
            const clickedLi = event.target.closest('li');
            if (clickedLi) {
                clickedLi.firstElementChild.classList.add('selected');
            }

            if (type === 'sd') {
                selectedSdFile = fileName;
                selectedUsbFile = null;
            } else {
                selectedUsbFile = fileName;
                selectedSdFile = null;
            }
            updateButtons();
        }

        function createListItem(fileName, type) {
            const li = document.createElement('li');
            const span = document.createElement('span');
            span.className = 'file-name';
            span.textContent = fileName;
            li.appendChild(span);
            const listElement = type === 'sd' ? sdList : usbList;
            li.onclick = () => selectFile(listElement, fileName, type);
            return li;
        }

        async function fetchFiles(type) {
            try {
                const response = await fetch(`/list-files?type=${type}`);
                const files = await response.json();
                const listElement = type === 'sd' ? sdList : usbList;
                listElement.innerHTML = '';
                if (files && files.length > 0) {
                  files.forEach(file => {
                    listElement.appendChild(createListItem(file.name, type));
                  });
                } else {
                   listElement.innerHTML = '<li>No books found.</li>';
                }
            } catch (e) {
                console.error(`Error fetching ${type} files:`, e);
                const listElement = type === 'sd' ? sdList : usbList;
                listElement.innerHTML = `<li>Error loading files. Check console.</li>`;
            }
        }
        
        async function transferFile(source, destination, fileName) {
            statusDiv.textContent = `Copying ${fileName}... This may take a moment.`;
            toReaderBtn.disabled = true;
            toLibraryBtn.disabled = true;
            try {
                const response = await fetch('/transfer-file', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ source, destination, filename: fileName })
                });
                const result = await response.json();
                statusDiv.textContent = result.message;
            } catch (e) {
                statusDiv.textContent = 'Error during transfer.';
            }
            // Refresh file lists after transfer
            setTimeout(() => {
              fetchFiles('sd');
              fetchFiles('usb');
              updateButtons();
            }, 1500);
        }

        function updateButtons() {
            toReaderBtn.disabled = !selectedSdFile;
            toLibraryBtn.disabled = !selectedUsbFile;
        }

        function checkStatus() {
            fetch('/status')
                .then(response => response.json())
                .then(data => {
                    const isConnected = data.reader_connected;
                    const wasConnected = statusDiv.classList.contains('connected');
                    
                    if (isConnected) {
                        statusDiv.textContent = 'E-Reader Connected!';
                        statusDiv.className = 'status connected';
                        if (!wasConnected) { // Fetch files only on new connection
                            fetchFiles('usb');
                        }
                    } else {
                        statusDiv.textContent = 'E-Reader Disconnected. Please plug in your device.';
                        statusDiv.className = 'status disconnected';
                        usbList.innerHTML = '<li>Please connect e-reader.</li>';
                    }
                });
        }

        toReaderBtn.onclick = () => {
            if (selectedSdFile) transferFile('sd', 'usb', selectedSdFile);
        };

        toLibraryBtn.onclick = () => {
            if (selectedUsbFile) transferFile('usb', 'sd', selectedUsbFile);
        };

        window.onload = () => {
            fetchFiles('sd');
            setInterval(checkStatus, 3000);
            checkStatus();
        };
    </script>
</body>
</html>
)rawliteral";
const char index_html_end[] = ""; // Dummy for sizeof calculation

// --- HELPER FUNCTIONS ---
// Helper to copy file between two filesystems
static esp_err_t copy_file(const char *source_path, const char *dest_path) {
    ESP_LOGI(TAG, "Copying from %s to %s", source_path, dest_path);
    FILE *source_file = fopen(source_path, "rb");
    if (!source_file) {
        ESP_LOGE(TAG, "Failed to open source file: %s", source_path);
        return ESP_FAIL;
    }

    FILE *dest_file = fopen(dest_path, "wb");
    if (!dest_file) {
        ESP_LOGE(TAG, "Failed to open destination file: %s", dest_path);
        fclose(source_file);
        return ESP_FAIL;
    }

    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), source_file)) > 0) {
        if (fwrite(buffer, 1, bytes_read, dest_file) != bytes_read) {
            ESP_LOGE(TAG, "Failed to write to destination file");
            fclose(source_file);
            fclose(dest_file);
            return ESP_FAIL;
        }
    }

    fclose(source_file);
    fclose(dest_file);
    ESP_LOGI(TAG, "File copied successfully");
    return ESP_OK;
}

// --- WEB SERVER HANDLERS ---
static esp_err_t main_page_handler(httpd_req_t *req) {
    httpd_resp_send(req, index_html_start, sizeof(index_html_start) - 1);
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    char resp_str[64];
    snprintf(resp_str, sizeof(resp_str), "{\"reader_connected\": %s}", ebook_reader_connected ? "true" : "false");
    httpd_resp_send(req, resp_str, strlen(resp_str));
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
    char content[256];
    int ret = httpd_req_recv(req, content, sizeof(content) -1);
    if (ret <= 0) {
        return ESP_FAIL;
    }
    content[ret] = '\0';
    
    cJSON *json = cJSON_Parse(content);
    if (!json) {
        httpd_resp_send_400(req);
        return ESP_FAIL;
    }

    const char *source = cJSON_GetObjectItem(json, "source")->valuestring;
    const char *destination = cJSON_GetObjectItem(json, "destination")->valuestring;
    const char *filename = cJSON_GetObjectItem(json, "filename")->valuestring;
    
    char source_path[256];
    char dest_path[256];

    snprintf(source_path, sizeof(source_path), "%s/%s", (strcmp(source, "sd") == 0) ? MOUNT_POINT_SD : MOUNT_POINT_USB, filename);
    snprintf(dest_path, sizeof(dest_path), "%s/%s", (strcmp(destination, "sd") == 0) ? MOUNT_POINT_SD : MOUNT_POINT_USB, filename);
    
    esp_err_t res = copy_file(source_path, dest_path);
    
    cJSON_Delete(json);

    cJSON *response_json = cJSON_CreateObject();
    cJSON_AddBoolToObject(response_json, "success", res == ESP_OK);
    cJSON_AddStringToObject(response_json, "message", res == ESP_OK ? "File transfer complete!" : "File transfer failed.");
    
    char *json_str = cJSON_PrintUnformatted(response_json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    cJSON_Delete(response_json);

    return ESP_OK;
}


// --- WEB SERVER SETUP ---
static httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t main_uri = { "/", HTTP_GET, main_page_handler, NULL };
        httpd_register_uri_handler(server, &main_uri);

        httpd_uri_t status_uri = { "/status", HTTP_GET, status_handler, NULL };
        httpd_register_uri_handler(server, &status_uri);
        
        httpd_uri_t list_uri = { "/list-files", HTTP_GET, list_files_handler, NULL };
        httpd_register_uri_handler(server, &list_uri);
        
        httpd_uri_t transfer_uri = { "/transfer-file", HTTP_POST, transfer_file_handler, NULL };
        httpd_register_uri_handler(server, &transfer_uri);
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
        return;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card VFS");
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
        ESP_ERROR_CHECK(msc_host_install_device(event->device, &device_handle));
        
        // Mount the filesystem
        ESP_ERROR_CHECK(vfs_msc_mount(MOUNT_POINT_USB, device_handle));
        ESP_LOGI(TAG, "MSC device mounted at %s", MOUNT_POINT_USB);

    } else if (event->event == MSC_DEVICE_DISCONNECTED) {
        ESP_LOGI(TAG, "MSC device disconnected");
        ebook_reader_connected = false;
        // Unmount the filesystem
        vfs_msc_unmount(MOUNT_POINT_USB);
        ESP_LOGI(TAG, "MSC device unmounted");
        msc_host_uninstall_device(device_handle);
    }
}

void usb_host_lib_task(void *arg)
{
    while (1) {
        // Start handling system events
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
        .callback = msc_event_cb,
    };
    ESP_ERROR_CHECK(msc_host_install(&msc_config));
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
    init_sd_card();
    init_wifi_ap();
    init_usb_host();

    // Start the web server
    start_webserver();

    ESP_LOGI(TAG, "E-Book Librarian is running!");
}
