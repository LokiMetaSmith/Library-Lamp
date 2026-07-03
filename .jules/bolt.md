## 2024-06-20 - [Avoid polling FAT filesystem metadata on ESP-IDF]
**Learning:** Polling `esp_vfs_fat_info` recursively traverses the FAT cluster chain to calculate free space. When called every second by frontend UI polling (like `/status`), this causes significant SPI bus latency and locks the VFS, creating a bottleneck that starves other FreeRTOS tasks (like network serving or HTTP handlers).
**Action:** When implementing status endpoints in ESP-IDF that report filesystem metrics, ALWAYS cache the result of `esp_vfs_fat_info` using `xTaskGetTickCount()` (e.g. for 10 seconds), only bypassing the cache when an active file transfer is known to be modifying the disk.

## 2024-06-22 - [Combine HTTP polling endpoints in ESP-IDF]
**Learning:** On ESP-IDF backend, executing concurrent HTTP polling requests via `Promise.all` directly to multiple different endpoints like `/audio/current` and `/audio/queue` creates significant overhead. The ESP-IDF `httpd` component is optimized for lower overhead when dealing with single endpoints rather than parallel request connections from the same client, which can cause connection pooling exhaustion and block main threads.
**Action:** When creating frontend UI interfaces for ESP-IDF devices, always combine grouped polling status queries (e.g. audio status, network status, queue data) into a unified `/state` endpoint rather than dispatching parallel `fetch()` connections to separate endpoints.

## 2024-10-27 - [Avoid Promise.all overhead for API fetches]
**Learning:** On ESP-IDF backend, executing concurrent HTTP polling requests via `Promise.all` directly to multiple different endpoints like `/list-files?type=sd` and `/list-files?type=usb` creates significant overhead. The ESP-IDF `httpd` component is optimized for lower overhead when dealing with single endpoints sequentially rather than parallel request connections from the same client, which can cause connection pooling exhaustion and block main threads.
**Action:** When fetching data from multiple endpoints in the frontend for ESP-IDF devices, always fetch them sequentially rather than in parallel with `Promise.all`.

## 2024-11-20 - [Increase static file chunk size to 4KB on ESP-IDF]
**Learning:** In ESP-IDF's `httpd` component, serving static files (like large HTML/JS/CSS assets from SPIFFS) using small chunks (e.g. 1024 bytes) incurs significant overhead due to the high number of context switches, VFS reads (`fread`), and `httpd_resp_send_chunk` calls.
**Action:** Always use larger chunk sizes (e.g., 4096 bytes) for static file handlers on the ESP32 to improve file transfer speed and reduce CPU usage, ensuring the buffer is heap-allocated (`malloc`) to prevent FreeRTOS task stack overflows.

## 2024-11-21 - [Avoid bypassing VFS cache during file transfers on ESP-IDF]
**Learning:** Polling `esp_vfs_fat_info` repeatedly during active file transfers (e.g., by bypassing the cache condition when `g_transfer_progress.active` is true) recursively traverses the FAT cluster chain and locks the VFS. This significantly slows down active file VFS operations like the transfer itself, drastically decreasing SPI bus availability and file transfer speeds.
**Action:** Even when a file transfer is active, rely on the time-based cache for `esp_vfs_fat_info` to prevent starving the SPI bus and throttling the transfer process itself.

## 2024-06-27 - [Stream large JSON files instead of loading into RAM on ESP32]
**Learning:** Loading large dynamically generated files like `catalog.json` entirely into RAM using `malloc(fsize + 1)` and `httpd_resp_send()` causes huge memory spikes and can crash the ESP32 due to heap exhaustion when the library grows.
**Action:** Always serve large files using `httpd_resp_send_chunk` with a fixed-size buffer (e.g. 4KB), even for API endpoints returning JSON, as the browser's `fetch` API transparently handles chunked transfer encoding without requiring frontend changes.

## 2024-11-23 - [Replace O(N) strncpy with memmove]
**Learning:** In C/ESP-IDF backend code, using a `for` loop with `strncpy` to shift array elements (like an audio queue) is O(N) time complexity and incurs unnecessary overhead by traversing each string to pad with zeroes.
**Action:** Always replace `for` loops that manually copy adjacent structures or strings with a single `memmove` operation to shift the entire block in memory. This is vastly faster and safer.

## 2024-05-19 - O(1) File Extension Checking
**Learning:** In C, `strstr(filename, ".ext")` performs an O(N*M) sequential substring search across the entire string, which is slow for directories with many files and causes bugs by incorrectly matching substrings in the middle of a filename (e.g., `book.epub.bak`).
**Action:** Always use `strrchr(filename, '.')` to jump directly to the extension and `strcasecmp` to check it in O(1) time for file validation.
## 2024-10-24 - Add Cache-Control for static assets
**Learning:** ESP32 web servers suffer significantly when repeatedly serving large static assets (like `vue.global.js` at 164KB) because the device's CPU, SPIFFS read speed, and network stack are heavily constrained. Browsers will re-request these files on every navigation without an explicit Cache-Control header.
**Action:** Always include a `Cache-Control` header (e.g. `public, max-age=31536000`) for large static dependencies in embedded web servers to allow the browser to skip the network request entirely, drastically improving page load times and reducing server load.

## 2024-05-18 - Consolidate Polled Endpoints to Prevent Connection Exhaustion
**Learning:** In ESP-IDF web servers, using parallel fetches (`Promise.all`) or sequential fetch chains to multiple endpoints exhausts the connection limit, causing `no slots left for registering handler` or high latency. Previously, `list-files?type=sd` and `list-files?type=usb` were fetched sequentially to avoid the parallel limitation, but this introduced "waterfall" latency on the frontend.
**Action:** When a frontend needs data from multiple backend sources on load, combine them into a single consolidated endpoint (e.g. `type=all`) that streams a combined JSON response using `httpd_resp_send_chunk`. This solves both the connection exhaustion issue and the frontend fetch latency penalty.
