# AI Agent Development Instructions for E-Book Librarian

## 1. Project Goal

Your primary objective is to continue the development of the **E-Book Librarian**, a standalone hardware device that allows users to share public domain e-books. The device uses an ESP32-S3 to read an e-reader's storage via USB Host, and a web server to transfer files between the e-reader and a local SD card library.

## 2. Current State

The project is currently a functional proof-of-concept. The core features have been implemented in a single `main.c` file.

- **Working Features:**
  - Wi-Fi Access Point creation.
  - Web server with a basic HTML interface for listing and transferring files.
  - SD card mounting and file access.
  - USB Host MSC driver for detecting and mounting an e-reader's storage.
  - WS2812 LED strip for visual status updates.

- **Project Files:**
  - `main/main.c`: Contains all application logic and the embedded web UI.
  - `main/CMakeLists.txt`: ESP-IDF build configuration for the main component.
  - `BOM.md`: A complete Bill of Materials for the hardware.
  - `wiring.yml`: A WireViz source file for the hardware wiring diagram.
  - `README.md`: Project overview and setup instructions.
  - `TODO.md`: A list of planned features.

## 3. Technology Stack

- **Hardware:** ESP32-S3 with USB OTG capability.
- **Framework:** ESP-IDF (C programming language).
- **Key Components:**
  - **RTOS:** FreeRTOS
  - **USB Host:** ESP-IDF USB Host Stack (specifically the Mass Storage Class driver).
  - **Web Server:** `esp_http_server`.
  - **LEDs:** `led_strip` component using the RMT peripheral.
  - **Filesystem:** `esp_vfs_fat` (FatFs) for SD card and USB partitions.
  - **JSON Parsing:** `cJSON`.

## 4. Development Tasks

Here are your next development tasks. Please complete them in order.

### Task 1: Refactor Web UI Assets

**Goal:** Decouple the web interface from the main application logic to improve maintainability.

**Instructions:**
1.  Create a new directory named `web_assets` inside the `main` directory.
2.  Extract the HTML, CSS, and JavaScript from the `index_html_start` raw literal in `main.c` into three separate files: `web_assets/index.html`, `web_assets/style.css`, and `web_assets/script.js`.
3.  Set up a SPIFFS filesystem partition for the project. Create a `spiffs.img` file containing the assets from the `web_assets` directory.
4.  Modify the `CMakeLists.txt` file to automatically flash this SPIFFS partition to the device.
5.  In `main.c`, remove the embedded HTML. Modify the `esp_http_server` setup to serve the static files (`index.html`, `style.css`, `script.js`) from the SPIFFS partition.

### Task 2: Implement File Transfer Progress with WebSockets

**Goal:** Provide real-time feedback to the user during file transfers.

**Instructions:**
1.  Add WebSocket support to the `esp_http_server`. Create a new WebSocket handler.
2.  Modify the `copy_file` function in `main.c`. It should now accept a WebSocket client ID as an argument. Inside its `while` loop, calculate the percentage of the file that has been copied.
3.  After writing each chunk, send a JSON message over the WebSocket connection containing the progress percentage (e.g., `{"type": "progress", "value": 55}`).
4.  In the web UI's JavaScript, establish a WebSocket connection to the server. When the user initiates a file transfer, listen for these progress messages and update a progress bar on the page.

### Task 3: Add E-Book Metadata Reading

**Goal:** Display user-friendly book titles and authors instead of filenames.

**Instructions:**
1.  Identify and integrate a lightweight, header-only C library for parsing `.zip` archives (since `.epub` files are essentially zip archives). `miniz` is a good candidate.
2.  Create a new function, `get_epub_metadata(const char *filepath)`, that opens an `.epub` file, unzips `META-INF/container.xml` to find the path to the `.opf` file, and then unzips and parses the `.opf` file (which is XML) to extract the `<dc:title>` and `<dc:creator>` tags.
3.  Modify the `/list-files` web handler. When it encounters a `.epub` file, it should call this new function and include the title and author in the JSON response to the client.
4.  Update the web UI to display the title and author, falling back to the filename if metadata is not available.
