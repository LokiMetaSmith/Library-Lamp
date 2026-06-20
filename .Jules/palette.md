## 2026-06-19 - [Form Accessibility]
**Learning:** Found several input fields relying solely on placeholders or lacking 'for' attributes on labels, which hurts screen reader users and reduces click targets.
**Action:** Always pair inputs with explicit <label for="..."> elements to improve both accessibility and usability.
## 2024-11-20 - Adding immediate visual feedback for submit actions
**Learning:** Vanilla JS frontends (like in `board.html`) often lack default form-submission loading states compared to component frameworks. Without immediate visual feedback (e.g., disabling the button, changing text to "POSTING..."), users may experience a perceived latency delay, especially when the backend process takes noticeable time, leading to potential duplicate submissions. Adding basic disabled styles (`opacity` & `cursor: not-allowed`) alongside JavaScript state toggles is a crucial low-hanging UX improvement for these raw setups.
**Action:** When adding async interaction endpoints in vanilla JS templates, always include a manual disabled/loading state toggle wrapping the `fetch()` call to improve perceived responsiveness and prevent double-posting.
