## YYYY-MM-DD - Ensure Buttons Re-enable Using `finally`
**Learning:** We noticed several buttons in `admin.html` and `script.js` (like POST/Upload/Cancel) disable themselves and append a `.loading-dots` state on submit. If the fetch request fails via network error or times out, missing `finally` blocks could cause the UI to remain stuck in this disabled loading state.
**Action:** When temporarily disabling a button for async loading indicators, always wrap the await call in a `try...catch...finally` block (or append `.finally()` to the promise chain) to cleanly re-enable the button and restore its descriptive aria-labels, guaranteeing graceful fallback even on dropped network connections.
## YYYY-MM-DD - Add Character Counters for Administrative Settings
**Learning:** Found that long text input fields under administrative settings in `admin.html` lacked length indicators compared to the community-facing forms in `board.html`.
**Action:** Always replicate useful character hint counts (`aria-describedby` links on input to visually styled `.char-hint` regions displaying length/max with `.warn` thresholding) on all admin panels where data limitations are strictly applied.
## 2024-03-24 - Preserving Empty State Tests
**Learning:** The testing suite enforces the presence of static 'empty state' strings directly in the HTML via regex or simple substring checking, even when a 'loading state' must appear first.
**Action:** When adding initial loading states to containers (like file lists or admin posts) using `innerHTML`, ensure the static empty state (or the element containing the string being tested) is either explicitly preserved in the overwritten HTML or hidden via `display: none` rather than completely removed from the DOM, so that CI end-to-end tests relying on strict string presence still pass natively.
## YYYY-MM-DD - Disabled State Context Loss
**Learning:** Found that when buttons were disabled to indicate loading state, their descriptive aria-label was being overwritten with temporary text (like "Loading..."). This causes context loss for screen reader users.
**Action:** When a button is disabled, do not overwrite its descriptive aria-label with temporary text. Keep the original descriptive label and allow the native disabled attribute to communicate the inactive state.
