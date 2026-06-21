# Performance & Architecture Roadmap

This document outlines the step-by-step plan for decoupling blocking operations (like SD card writes and LoRaWAN transmissions) from the web server by migrating them to dedicated background FreeRTOS tasks.

## Goal
Improve the perceived performance of the E-Book Librarian web interface by preventing long-running hardware tasks from blocking the HTTP request handlers. The ESP32-S3's dual cores will be utilized to handle background operations concurrently.

## Todo List & Stages

### Stage 1: Documentation & Planning
- [x] Create this `PERFORMANCE_ROADMAP.md` document.

### Stage 2: Backend FreeRTOS Architecture
- [ ] Create a FreeRTOS message queue for handling asynchronous `catalog.json` updates.
- [ ] Create a FreeRTOS message queue for handling LoRaWAN broadcasts.
- [ ] Implement `catalog_update_task` pinned to Core 1 to process catalog updates in the background.
- [ ] Implement `lora_background_task` pinned to Core 1 to handle LoRaWAN broadcasts and run a periodic "discovery heartbeat".

### Stage 3: Refactor Synchronous Handlers
- [ ] Modify `upload_handler` to enqueue catalog updates rather than writing directly to `catalog.json`.
- [ ] Modify `lora_wan_broadcast` (and related functions) to enqueue transmissions instead of blocking the caller.
- [ ] Implement state variables to track the status of background tasks (e.g., `catalog_updating`, `lora_scanning`).
- [ ] Update the `/status` API endpoint to return these background task states.

### Stage 4: Frontend UI Updates
- [ ] Add UI elements (loading spinners/banners) in `main/web_assets/index.html` (or similar) to indicate when the catalog is updating or LoRaWAN is scanning.
- [ ] Modify the frontend JavaScript (`script.js` etc.) to read the new status fields from the `/status` endpoint and toggle the new UI indicators dynamically.

### Stage 5: Verification & Pre-commit
- [ ] Verify that HTTP requests no longer block during uploads.
- [ ] Complete pre-commit checklist.
- [ ] Submit PR.
