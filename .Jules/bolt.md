## 2024-06-19 - Pure UI State Reloading Data
**Learning:** In standard HTML/JS applications without reactive frameworks, UI sorting actions (like clicking "New" or "Expiring") can easily be hardcoded to trigger redundant full data reloads (e.g., `load(); loadSystemStatus();`) instead of re-rendering cached data. This causes severe, unnecessary backend load and UI latency for simple list sorts.
**Action:** When inspecting client-side sorting and filtering, check if the data is being fetched anew. Always reuse cached data (e.g., `lastData`) for operations that only change the presentation order.

## 2024-08-01 - Synchronous Hardware Transmission in GET Handlers
**Learning:** Placing synchronous hardware transmissions (like `lora_wan_broadcast`) inside frequent, view-only HTTP GET handlers (like `/list-files`) acts as a massive performance bottleneck. It unnecessarily blocks the HTTP thread for 500-1500ms on every UI refresh and causes severe spectrum pollution.
**Action:** When inspecting API handlers, ensure that expensive hardware side-effects are only executed during mutating state changes (e.g., POST uploads) rather than during routine read operations.
