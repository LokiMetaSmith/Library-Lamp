# Performance & Architecture Roadmap

This document outlines the step-by-step plan for decoupling blocking operations (like SD card writes and LoRaWAN transmissions) from the web server by migrating them to dedicated background FreeRTOS tasks.

## Goal
Improve the perceived performance of the E-Book Librarian web interface by preventing long-running hardware tasks from blocking the HTTP request handlers. The ESP32-S3's dual cores will be utilized to handle background operations concurrently.

## Todo List & Stages

### Stage 1: Documentation & Planning
- [x] Create this `PERFORMANCE_ROADMAP.md` document.

### Stage 2: Backend FreeRTOS Architecture
- [x] Create a FreeRTOS message queue for handling asynchronous `catalog.json` updates.
- [x] Create a FreeRTOS message queue for handling LoRaWAN broadcasts.
- [x] Implement `catalog_update_task` pinned to Core 1 to process catalog updates in the background.
- [x] Implement `lora_background_task` pinned to Core 1 to handle LoRaWAN broadcasts and run a periodic "discovery heartbeat".

### Stage 3: Refactor Synchronous Handlers
- [x] Modify `upload_handler` to enqueue catalog updates rather than writing directly to `catalog.json`.
- [x] Modify `lora_wan_broadcast` (and related functions) to enqueue transmissions instead of blocking the caller.
- [x] Implement state variables to track the status of background tasks (e.g., `catalog_updating`, `lora_scanning`).
- [x] Update the `/status` API endpoint to return these background task states.

### Stage 4: Frontend UI Updates
- [x] Add UI elements (loading spinners/banners) in `main/web_assets/index.html` (or similar) to indicate when the catalog is updating or LoRaWAN is scanning.
- [x] Modify the frontend JavaScript (`script.js` etc.) to read the new status fields from the `/status` endpoint and toggle the new UI indicators dynamically.

### Stage 5: Verification & Pre-commit
- [x] Verify that HTTP requests no longer block during uploads.
- [x] Complete pre-commit checklist.
- [x] Submit PR.
