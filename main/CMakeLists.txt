# CMake build script for the main component of the E-Book Librarian project.

# List of source files for this component.
set(COMPONENT_SRCS "main.c")

# List of directories to add to the include path.
set(COMPONENT_ADD_INCLUDEDIRS ".")

# Register the component with the ESP-IDF build system.
# This specifies the source files and the dependencies (other components) it requires.
idf_component_register(SRCS "${COMPONENT_SRCS}"
                       INCLUDE_DIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                       REQUIRES
                                # Core IDF components
                                nvs_flash
                                esp_wifi
                                esp_event
                                esp_log
                                esp_system
                                freertos

                                # Filesystem components
                                esp_vfs_fat
                                sdmmc
                                spiffs

                                # Networking components
                                esp_http_server

                                # USB Host components
                                usb

                                # Peripherals
                                led_strip

                                # Utility components
                                cJSON
                       )

# Generate the SPIFFS image from the web_assets directory
set(SPIFFS_IMAGE_DIR "${CMAKE_CURRENT_LIST_DIR}/web_assets")
spiffs_create_partition_image(storage ${SPIFFS_IMAGE_DIR})
