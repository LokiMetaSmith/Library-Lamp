## 2024-06-25 - Dynamic Feedback Accessibility in Vanilla JS
**Learning:** In vanilla HTML/JS frontends, asynchronous status messages (like success/error toasts) injected directly into empty DOM nodes are completely invisible to screen readers without ARIA attributes.
**Action:** When creating or modifying dynamic text feedback containers (`.feedback`, `.error`), always add `aria-live="polite"` for non-disruptive updates and `aria-live="assertive"` for critical errors to ensure screen readers announce the changes automatically.
