## YYYY-MM-DD - Ensure Buttons Re-enable Using `finally`
**Learning:** We noticed several buttons in `admin.html` and `script.js` (like POST/Upload/Cancel) disable themselves and append a `.loading-dots` state on submit. If the fetch request fails via network error or times out, missing `finally` blocks could cause the UI to remain stuck in this disabled loading state.
**Action:** When temporarily disabling a button for async loading indicators, always wrap the await call in a `try...catch...finally` block (or append `.finally()` to the promise chain) to cleanly re-enable the button and restore its descriptive aria-labels, guaranteeing graceful fallback even on dropped network connections.
## YYYY-MM-DD - Add Character Counters for Administrative Settings
**Learning:** Found that long text input fields under administrative settings in `admin.html` lacked length indicators compared to the community-facing forms in `board.html`.
**Action:** Always replicate useful character hint counts (`aria-describedby` links on input to visually styled `.char-hint` regions displaying length/max with `.warn` thresholding) on all admin panels where data limitations are strictly applied.
