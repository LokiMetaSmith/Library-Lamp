## 2026-06-19 - [Form Accessibility]
**Learning:** Found several input fields relying solely on placeholders or lacking 'for' attributes on labels, which hurts screen reader users and reduces click targets.
**Action:** Always pair inputs with explicit <label for="..."> elements to improve both accessibility and usability.
## 2024-11-20 - Adding immediate visual feedback for submit actions
**Learning:** Vanilla JS frontends (like in `board.html`) often lack default form-submission loading states compared to component frameworks. Without immediate visual feedback (e.g., disabling the button, changing text to "POSTING..."), users may experience a perceived latency delay, especially when the backend process takes noticeable time, leading to potential duplicate submissions. Adding basic disabled styles (`opacity` & `cursor: not-allowed`) alongside JavaScript state toggles is a crucial low-hanging UX improvement for these raw setups.
**Action:** When adding async interaction endpoints in vanilla JS templates, always include a manual disabled/loading state toggle wrapping the `fetch()` call to improve perceived responsiveness and prevent double-posting.
## 2024-06-21 - [Form Accessibility]
**Learning:** Vanilla HTML forms in this project's UI (like `index.html`) sometimes lack explicit programmatic associations between labels and input fields. Using only placeholders or unlinked labels reduces the click target area and degrades the experience for screen reader users.
**Action:** When working on UI forms, ensure all input elements (`<input>`) have explicit `id` attributes that are strictly matched with `for` attributes on their corresponding `<label>` elements.
## 2026-06-22 - Explicit Form Labels for Accessibility
**Learning:** Found an accessibility issue pattern where inputs used only `placeholder` and `aria-label` attributes instead of explicit, visible `<label>` tags (e.g., in `setup.html` and the `admin.html` password gate). While `aria-label` provides screen reader context, relying solely on placeholder text is poor UX because the context disappears as soon as the user starts typing, which can be disorienting and fails to meet WCAG success criteria for visible labels.
**Action:** Always pair inputs with visible, explicit `<label>` tags using the `for` and `id` attributes to ensure both visual persistence and screen reader compatibility.
