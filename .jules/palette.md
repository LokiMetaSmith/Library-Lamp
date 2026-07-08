## 2026-06-19 - [Form Accessibility]
**Learning:** Found several input fields relying solely on placeholders or lacking 'for' attributes on labels, which hurts screen reader users and reduces click targets.
**Action:** Always pair inputs with explicit <label for="..."> elements to improve both accessibility and usability.
## 2024-07-04 - [Board Category Accessibility]
**Learning:** When using custom button groups instead of standard `<select>` or radio buttons for form inputs (like the Category selector on the board), screen readers lose the context that the label applies to the entire group of buttons.
**Action:** Always wrap custom button groups in an element with `role="group"` and use `aria-labelledby` pointing to the ID of the visual label to maintain accessibility context.
## 2024-11-20 - Adding immediate visual feedback for submit actions
**Learning:** Vanilla JS frontends (like in `board.html`) often lack default form-submission loading states compared to component frameworks. Without immediate visual feedback (e.g., disabling the button, changing text to "POSTING..."), users may experience a perceived latency delay, especially when the backend process takes noticeable time, leading to potential duplicate submissions. Adding basic disabled styles (`opacity` & `cursor: not-allowed`) alongside JavaScript state toggles is a crucial low-hanging UX improvement for these raw setups.
**Action:** When adding async interaction endpoints in vanilla JS templates, always include a manual disabled/loading state toggle wrapping the `fetch()` call to improve perceived responsiveness and prevent double-posting.
## 2024-06-21 - [Form Accessibility]
**Learning:** Vanilla HTML forms in this project's UI (like `index.html`) sometimes lack explicit programmatic associations between labels and input fields. Using only placeholders or unlinked labels reduces the click target area and degrades the experience for screen reader users.
**Action:** When working on UI forms, ensure all input elements (`<input>`) have explicit `id` attributes that are strictly matched with `for` attributes on their corresponding `<label>` elements.
## 2026-06-22 - Explicit Form Labels for Accessibility
**Learning:** Found an accessibility issue pattern where inputs used only `placeholder` and `aria-label` attributes instead of explicit, visible `<label>` tags (e.g., in `setup.html` and the `admin.html` password gate). While `aria-label` provides screen reader context, relying solely on placeholder text is poor UX because the context disappears as soon as the user starts typing, which can be disorienting and fails to meet WCAG success criteria for visible labels.
**Action:** Always pair inputs with visible, explicit `<label>` tags using the `for` and `id` attributes to ensure both visual persistence and screen reader compatibility.
## 2026-06-23 - Empty states in data lists
**Learning:** Found an interaction improvement opportunity where lists displaying dynamic data (like `localFiles` and `ereaderFiles` in `index.html`) lacked empty states. When a user has no files, displaying nothing leaves them unsure if the application is broken, loading, or genuinely empty. Adding empty states using `v-if` with clear, helpful messaging removes ambiguity and matches the established UX pattern in other areas like `audio.html`.
**Action:** When working on UI lists or data tables that can be empty, always ensure there is a clear, styled empty state providing context to the user.
## 2026-06-24 - Accessibility for Toggle Buttons
**Learning:** Found an interaction improvement opportunity in `board.html` where filtering, sorting, and category buttons visually indicated their active state via CSS classes (e.g., `active` or `active-Notice`), but this state wasn't communicated to screen readers. This leaves visually impaired users unaware of which filter or category is currently selected.
**Action:** When creating toggle buttons or selectable tabs, always pair visual state classes with `aria-pressed="true"` or `aria-pressed="false"` attributes, and ensure these attributes are dynamically updated alongside the CSS classes in JavaScript logic.
## 2024-05-24 - Dynamic Disabled State Explanations with Keyboard Hints
**Learning:** Adding static aria-labels and static titles to buttons isn't enough when their state changes (e.g. from disabled to enabled). Users need to know *why* a button is disabled, and screen readers need an aria-label. Furthermore, when the button is enabled, replacing the tooltip with keyboard shortcut hints greatly improves usability without cluttering the UI.
**Action:** When creating interactive viewer-like experiences, provide a default `title` explaining the loading/disabled state along with a static `aria-label`. Then, dynamically update the `title` attribute via JavaScript to show keyboard shortcuts once the state becomes active.
## 2024-11-21 - Passing element context in vanilla JS lists
**Learning:** When rendering dynamic lists with string interpolation in vanilla JS (like in `admin.html`), it can be tricky to grab the exact button element later to apply loading states, especially if the button lacks a unique ID.
**Action:** When adding inline onclick handlers in template literals (e.g., `onclick="deletePost(${id}, this)"`), pass `this` to give the function a direct reference to the clicked element, making it easy to toggle disabled states and text content immediately.

## 2024-11-21 - [Vanilla JS Silent Form Validations]
**Learning:** In vanilla JavaScript frontends, simply returning early (`if (!value) return;`) on empty form submissions creates a silent failure where the user clicks the button but nothing happens, which is confusing and poor UX/a11y.
**Action:** When catching empty required inputs in vanilla JS functions, always use the existing `aria-live` hint/error containers to display a clear error message, and immediately call `.focus()` on the empty input to guide the user to the required action.
