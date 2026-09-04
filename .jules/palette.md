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

## 2026-12-11 - [Visible Character Limits in Text Inputs]
**Learning:** Character count hints on input fields that only appear when the user is close to the limit (e.g., `n > 18` for a 24-character max) can leave the user guessing their remaining characters for most of the typing experience.
**Action:** When creating forms with character limits on inputs, always display the character count explicitly (e.g., "0/24") right from the beginning and update it continuously as the user types, rather than hiding it conditionally.

## 2026-12-11 - [ARIA Labels for Download Links]
**Learning:** When using standard text links (like book titles) as download triggers, screen readers may announce just the title without clarifying the action. Adding an explicit `aria-label` (e.g., "Download The Great Gatsby") provides necessary context for visually impaired users.
**Action:** Always append an explicit `aria-label` describing the action to text links that initiate downloads, especially in dynamic lists.

## 2026-12-11 - [ARIA Titles for Icon-Only Buttons]
**Learning:** Icon-only buttons (like the hamburger menu) that rely on `aria-label` for screen reader accessibility often lack clear explanations for sighted users on hover, especially keyboard users. This reduces discoverability and can cause confusion about the button's function.
**Action:** Always complement `aria-label` on icon-only buttons with a matching `title` attribute to provide a helpful tooltip for sighted mouse and keyboard users.

## 2026-12-11 - [Empty State Calls to Action]
**Learning:** Found an empty state for a disconnected E-Reader that just said "No e-reader connected." This lacked helpful guidance for users, similar to a previous finding where an empty library lacked a call-to-action.
**Action:** When creating empty states, always reuse the `.empty-state` CSS class for consistency, and provide a helpful call-to-action or instructions (e.g., "Connect your device via USB to view and transfer books.") rather than just stating the lack of data.

## 2026-08-25 - [Inline Form Validation vs Native Tooltips]
**Learning:** When using custom `aria-live` inline validation messages, retaining standard browser validation (via the `required` attribute) can cause native tooltips to trigger, silently blocking form submission and preventing custom JS logic from firing. The form submission logic previously used a confusing loop between `onsubmit` and `onclick` just to call `checkValidity()`, resulting in a poor experience and broken feedback.
**Action:** When implementing custom inline form validation, always add the `novalidate` attribute to the `<form>` tag to disable native tooltips, while keeping the semantic `required` attributes on inputs for screen readers. Use a clean `onsubmit="event.preventDefault(); doCustomLogic();"` approach.

## 2026-12-11 - [Custom Form Validation Conflicts]
**Learning:** Implementing custom JavaScript validation () on forms with  inputs can cause browsers to block submission silently due to native HTML5 validation tooltips failing to appear or interfering with the custom logic.
**Action:** When overriding form submission to use custom inline validation feedback, always add the `novalidate` attribute to the `<form>` element. This disables the native browser tooltips while keeping the semantic `required` attributes for screen readers.

## 2026-12-11 - [Custom Form Validation Conflicts]
**Learning:** Implementing custom JavaScript validation on forms with required inputs can cause browsers to block submission silently due to native HTML5 validation tooltips failing to appear or interfering with the custom logic.
**Action:** When overriding form submission to use custom inline validation feedback, always add the `novalidate` attribute to the form element. This disables the native browser tooltips while keeping the semantic required attributes for screen readers.

## 2026-12-11 - [Visible Character Limits Reset in Forms]
**Learning:** When resetting forms that include visible character limit hints (e.g., '0/300'), ensure the hint text is explicitly reset to its initial count rather than being cleared entirely, which would incorrectly hide the indicator from the user for their next entry.
**Action:** Always reset character hint containers explicitly back to their default state (e.g., `'0/300'`) upon form submission, preventing the count from silently disappearing.

## 2026-12-12 - [Improved Empty State Guidance in Admin Portal]
**Learning:** Found that the post empty states in `admin.html` lacked helpful contextual guidance and failed to reuse the consistent `.empty-state` CSS class, contrary to previous learnings on other components.
**Action:** When managing empty states across all pages, always append `.empty-state` for layout consistency and provide actionable or explanatory subtext (e.g., 'Community posts will appear here for moderation.') rather than merely stating that no data is present.

## 2026-12-12 - [Bulletin Board Empty State Enhancement]
**Learning:** Found an empty state in `board.html` that lacked the global `.empty-state` CSS class and used plain text rather than the standardized `.file-notes` subtext class, leading to an inconsistent empty state appearance compared to the rest of the app.
**Action:** When defining empty states, always append `.empty-state` for layout consistency, retaining any necessary specific layout classes (like `.empty` for grid column spanning). Additionally, wrap subtext instructions in `.file-notes` to match the application's design system.

## 2026-12-13 - [Empty State Subtext Consistency]
**Learning:** We discovered multiple instances across the frontend where secondary subtext or helpful instructions inside `.empty-state` containers were using hardcoded inline styles (like `font-size: 0.9em; margin-top: 5px;`) instead of adhering to the design system's consistent `.file-notes` class.
**Action:** When providing secondary subtext or helpful instructions within UI components (such as `.empty-state` containers), always use the existing `.file-notes` CSS class instead of inline styles to maintain design system consistency.

## 2026-12-14 - [ARIA Live Regions for Transient States]
**Learning:** When implementing transient full-screen loading overlays or viewer error states, applying `aria-live` directly to an element that toggles its `display` property can cause screen readers to fail to announce the state change reliably.
**Action:** Always wrap dynamically updating status text (like loading indicators or error banners) in a permanent `aria-live="polite"` or `aria-live="assertive"` container so screen readers correctly detect and announce the visibility or text changes.
