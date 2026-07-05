
# List of source files for this component.
set(COMPONENT_SRCS "main.c" "dns_server.c" "lorawan.cpp" "bulletin_board.c" "bulletin_api.c" "audio_api.c")

# List of directories to add to the include path.
set(COMPONENT_ADD_INCLUDEDIRS ".")

# Embed the setup page directly into the firmware binary.
# This allows it to be served in AP mode before SPIFFS is mounted.
# # target_add_binary_data(app "web_assets/setup.html" TEXT)

# Register the component with the ESP-IDF build system.
# This specifies the source files and the dependencies (other components) it requires.
idf_component_register(SRCS "${COMPONENT_SRCS}"
                    INCLUDE_DIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                    # EMBED_TXTFILES "web_assets/setup.html"
                    REQUIRES
                         # Core IDF components
                         nvs_flash
                         esp_wifi
                         esp_event
                         esp_system
                         usb
                         freertos
                         esp_common  # Good practice to include for driver dependencies
                         esp_timer
                         esp_hw_support

                         # Filesystem components
                         fatfs
                         sdmmc
                         spiffs

                         # Networking components
                         esp_http_server

                         # USB Host components
                         usb
                         usb_host_msc

                         # Peripherals
                         # RMT and GPIO drivers are typically brought in by esp_common or other components

                         esp_driver_rmt
                         # Bluetooth components
                         bt

                         # External Components
                         radiolib
                         cjson
                         sqlite3
)
# Generate the SPIFFS image from the web_assets directory
set(SPIFFS_IMAGE_DIR "${CMAKE_CURRENT_LIST_DIR}/web_assets")
spiffs_create_partition_image(storage ${SPIFFS_IMAGE_DIR})
