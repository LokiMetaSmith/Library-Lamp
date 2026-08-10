## 2026-10-27 - [Vanilla JS Async Feedback for Hardware State Changes]
**Learning:** Adding loading states to vanilla JS buttons that trigger an action which eventually halts the backend (like a sleep command) requires special care. If you reset the loading state to false in a `finally` block, the button may briefly flash back to its active state before the hardware actually goes to sleep and stops responding.
**Action:** When handling async operations that trigger a page reload, navigation, or hardware halt, only reset the loading state in the `catch` block so the UI remains in the disabled/loading state until the system physically stops responding.## 2026-10-27 - [Vanilla JS Async Feedback for Hardware State Changes]
**Learning:** Adding loading states to vanilla JS buttons that trigger an action which eventually halts the backend (like a sleep command) requires special care. If you reset the loading state to false in a `finally` block, the button may briefly flash back to its active state before the hardware actually goes to sleep and stops responding.
**Action:** When handling async operations that trigger a page reload, navigation, or hardware halt, only reset the loading state in the `catch` block so the UI remains in the disabled/loading state until the system physically stops responding.
## 2026-10-28 - [Color Contrast for Empty/Disabled States]
**Learning:** The hardcoded color `#8a7b6c` was being used across the app for disabled buttons and empty states, which fails WCAG AA contrast ratio (3.44:1) against the dark theme backgrounds.
**Action:** Always use the existing `--ink-muted` CSS variable (or `#d1bfae`) for muted text to ensure sufficient contrast (5.02:1) while maintaining the visual hierarchy.

## 2026-12-10 - Implicit Form Submission
**Learning:** Found a custom form implementation in `board.html` that used a `div` container and a `<button onclick="...">` for submission, but missed out on native browser features like implicit submission (submitting via the Enter key in an input) and native HTML5 validation UI.
**Action:** When creating forms, always wrap the inputs in a `<form>` element and use a `type="submit"` button. For custom button groups inside a form, ensure they have `type="button"` so they don't accidentally trigger submission. Use `onsubmit="event.preventDefault(); ..."` to handle the submission via JS while keeping the accessible native features.

## 2026-12-10 - Implicit Form Submission
**Learning:** In custom admin panel layouts (e.g., `admin.html`), using `<div>` wrappers instead of `<form>` elements prevents users from submitting forms using the Enter key natively and bypasses built-in HTML5 validation like `minlength`.
**Action:** When creating form inputs, always wrap them in a `<form>` tag and include a `<button type="submit">`. Use `onsubmit="event.preventDefault(); ..."` to intercept the action while maintaining standard accessibility and browser behaviors.

## 2026-12-11 - [Input Placeholder Examples in Forms]
**Learning:** Adding `placeholder` attributes with clear examples (e.g., "e.g., Neighborhood Library") to form inputs significantly improves the UX of configuration pages by reducing cognitive load and demonstrating the expected data format.
**Action:** When adding or updating configuration or management forms, always provide contextual `placeholder` text on empty input fields to guide the user.

## 2026-12-11 - [Input Placeholder Examples in Setup Form]
**Learning:** Adding `placeholder` attributes with clear examples (e.g., "e.g., MyHomeNetwork" and "e.g., MySecretPassword") to configuration inputs in `setup.html` significantly improves the UX by reducing cognitive load and demonstrating the expected data format for the hardware portal.
**Action:** When creating or maintaining configuration and setup forms, always ensure text and password inputs have descriptive `placeholder` attributes.
## 2026-12-11 - [Implicit Form Submission in Admin Portal]
**Learning:** The "Board Identity" configuration section in the Admin portal used a generic `<div>` wrapper with a standard button calling a JavaScript function on click. This setup prevented users from pressing the "Enter" key to submit the form, which is a common expectation for multi-input forms. By replacing the `<div>` with a `<form>` tag, adding an `onsubmit` handler with `event.preventDefault()`, and changing the button to `type="submit"`, native implicit form submission is enabled while retaining the existing JavaScript logic and CSS styling.
**Action:** When building or updating multi-input configuration sections that require submission, always use semantic `<form>` tags and `type="submit"` buttons, handling the event via `onsubmit` rather than `onclick` to ensure native keyboard accessibility.
