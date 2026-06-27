## 2024-06-25 - Dynamic Feedback Accessibility in Vanilla JS
**Learning:** In vanilla HTML/JS frontends, asynchronous status messages (like success/error toasts) injected directly into empty DOM nodes are completely invisible to screen readers without ARIA attributes.
**Action:** When creating or modifying dynamic text feedback containers (`.feedback`, `.error`), always add `aria-live="polite"` for non-disruptive updates and `aria-live="assertive"` for critical errors to ensure screen readers announce the changes automatically.
## 2024-06-26 - Add aria-live to dynamic feedback
**Learning:** Screen readers won't announce dynamic text changes (like error banners, status updates, or character count hints) in SPAs and interactive forms unless explicitly marked. Vanilla JS state changes can be silent to assistive technologies.
**Action:** Always add `aria-live="assertive"` to critical error banners/messages and `aria-live="polite"` to status updates and hints in vanilla HTML/JS frontends when the DOM node dynamically updates text content.
## 2024-05-24 - Empty States and Disabled Buttons
**Learning:** Empty states without actionable guidance leave users wondering what to do next. Disabled buttons without tooltips are confusing. Adding a simple CTA (like "Upload a book") to an empty list and `title` tooltips to disabled buttons significantly improves the experience.
**Action:** Always combine empty states with a helpful CTA when possible. Whenever disabling a button, provide a tooltip (`title` attribute) explaining *why* it's disabled or what action is currently happening.
