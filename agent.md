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

*   **Framework:** ESP-IDF (via PlatformIO)
*   **Language:** C
*   **Key Libraries/Components:**
    *   **esp_http_server:** For the web interface.
    *   **esp_vfs_fat & USB Host MSC:** For managing the SD card and e-reader filesystems.
    *   **led_strip:** For controlling the RGB LED strip.
    *   **cJSON:** For handling JSON data in web requests.
    *   **FreeRTOS:** For multitasking (e.g., running the web server and USB host stack concurrently).

## Project Structure

*   `main/`: Contains the main application source code (`main.c`).
*   `main/web_assets/`: (To be created) Will contain the HTML, CSS, and JS for the web UI.
*   `platformio.ini`: PlatformIO project configuration file.
*   `BOM.md`: Bill of Materials for the hardware.
*   `README.md`: General project documentation.
*   `agent.md`: This file.

## Environment Setup

This project uses PlatformIO, which simplifies the ESP-IDF toolchain management.

1.  **Install PlatformIO:** Ensure you have PlatformIO Core installed or are using an IDE with the PlatformIO extension (like VS Code). The extension will handle the installation of the necessary toolchains and frameworks automatically.
2.  **Project Dependencies:** PlatformIO will automatically download the `espressif32` platform and ESP-IDF version specified in `platformio.ini` the first time you build the project. No manual installation of ESP-IDF is required.

## Building and Flashing

Use the following PlatformIO commands to build and upload the project to the ESP32-S3 board.

1.  **Build the project:**
    ```bash
    pio run
    ```
2.  **Upload the firmware:**
    ```bash
    pio run --target upload
    ```
3.  **Build, upload, and open the serial monitor:**
    ```bash
    pio run -t upload -t monitor
    ```

## Working with the Code

### Development Tasks

Your next development tasks are outlined below. Please complete them in order.

**Task 1: Refactor Web UI Assets**

*   **Goal:** Decouple the web interface from the main application logic.
*   **Instructions:**
    1.  Create a `main/web_assets` directory.
    2.  Extract the HTML, CSS, and JavaScript from the `index_html_start` raw literal in `main.c` into `index.html`, `style.css`, and `script.js` inside `main/web_assets`.
    3.  Create a SPIFFS partition to store these assets. You will need to create a `spiffs.img` and configure PlatformIO to flash it.
    4.  Modify `main.c` to serve the static files from SPIFFS instead of the embedded string.

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