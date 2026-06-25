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
