# Agent Instructions

This file contains instructions for AI agents working with this codebase.

## Project Overview

**Project Name:** Library-Lamp (E-Book Librarian)

**High-Level Goal:** To create a standalone hardware device that serves as a physical, shareable library of public domain e-books. The device allows users to connect e-readers via USB and transfer books to and from a local library stored on an SD card, all managed through a simple web interface.

**Core Functionality:** The system hosts its own Wi-Fi access point and a web server, allowing any Wi-Fi enabled device (phone, computer) to manage the library without an internet connection.

For more detailed information, please refer to the `README.md`.

## Hardware Stack

A full list of components is available in `BOM.md`.

*   **MCU:** ESP32-S3 Development Board with USB OTG (e.g., ESP32-S3-USB-OTG)
*   **Storage:** MicroSD Card Module with a MicroSD card (exFAT formatted for >32GB).
*   **Visuals:** WS2812B (NeoPixel) RGB LED Strip for status indication.
*   **Interfacing:** USB OTG adapter/cable to connect to e-readers.

## Software Architecture

*   **Framework:** ESP-IDF
*   **Language:** C
*   **Key Libraries/Components:**
    *   **esp_http_server:** For the web interface.
    *   **espressif/usb_host_msc:** Managed component for USB Mass Storage.
    *   **led_strip:** For controlling the RGB LED strip.
    *   **cJSON:** For handling JSON data in web requests.
    *   **FreeRTOS:** For multitasking.

## Project Structure

*   `main/`: Contains the main application source code (`main.c`).
*   `main/web_assets/`: Contains the HTML, CSS, and JS for the web UI.
*   `main/idf_component.yml`: Manifest for managed dependencies.
*   `components/`: For local components like `sqlite3`.
*   `partitions.csv`: Defines the flash memory layout.
*   `sdkconfig`: Project configuration file.
*   `CMakeLists.txt`: Top-level build script for ESP-IDF.
*   `agent.md`: This file.

## Environment Setup

This project requires the ESP-IDF toolchain. The necessary tools are included in the environment, but they must be installed and activated first.

1.  **Install System Dependencies:** This project requires `libusb` for the `openocd` tool. It must be installed first:
    ```bash
    sudo apt-get update && sudo apt-get install -y libusb-1.0-0
    ```

2.  **Install ESP-IDF Tools:** The ESP-IDF installation script must be run to set up the Python virtual environment and toolchains.
    ```bash
    chmod +x ~/.platformio/packages/framework-espidf/install.sh
    ~/.platformio/packages/framework-espidf/install.sh
    ```

3.  **Activate the Environment:** Before building, you must activate the ESP-IDF environment in your shell session by sourcing the `export.sh` script:
    ```bash
    source ~/.platformio/packages/framework-espidf/export.sh
    ```
    After this, the `idf.py` command will be available.

## Building and Flashing

After setting up and activating the environment, use the following `idf.py` commands to build and flash the project.

1.  **Build the project:**
    ```bash
    idf.py build
    ```

2.  **Flash the project:**
    ```bash
    idf.py flash
    ```

3.  **Build, flash, and monitor:**
    ```bash
    idf.py flash monitor
    ```

## Working with the Code

### Development Tasks

Your next development tasks are outlined below. Please complete them in order.

**Task 1: Refactor Web UI Assets**

*   **Goal:** Decouple the web interface from the main application logic.
*   **Instructions:**
    1.  Create a `main/web_assets` directory.
    2.  Extract the HTML, CSS, and JavaScript from the `index_html_start` raw literal in `main.c` into `index.html`, `style.css`, and `script.js` inside `main/web_assets`.
    3.  The project is already configured to build a SPIFFS image from this directory.

**Task 2: Implement File Transfer Progress with WebSockets**

*   **Goal:** Provide real-time feedback during file transfers.
*   **Instructions:**
    1.  Add WebSocket support to the `esp_http_server`.
    2.  Update the `copy_file` function to calculate and send progress updates over the WebSocket connection.
    3.  Implement a progress bar in the JavaScript UI that updates based on messages from the WebSocket.

**Task 3: Add E-Book Metadata Reading**

*   **Goal:** Display book titles and authors instead of raw filenames.
*   **Instructions:**
    1.  Integrate a lightweight C library for parsing `.epub` files (which are zip archives).
    2.  Create a function to extract the title and author from an `.epub` file's metadata (`.opf` file).
    3.  Modify the `/list-files` API endpoint to include this metadata in the JSON response.
    4.  Update the web UI to display the title and author.