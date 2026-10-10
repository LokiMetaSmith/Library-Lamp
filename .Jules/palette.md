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
## 2024-12-05 - [Item-Specific Loading States in Vue]
**Learning:** When users click action buttons inside a dynamic Vue list (like adding or removing tracks in a queue), failing to provide item-specific loading states can result in double-clicks that spam the backend or cause confusion. Binding specific identifiers (like the filename or index) to a tracking variable provides immediate, targeted visual feedback (e.g., disabling only the clicked button).
**Action:** Always implement and bind item-specific loading states (e.g., `this.addingToQueue = filename`) for asynchronous actions triggered within `v-for` lists.
## 2024-07-28 - Keyboard Exit Shortcuts in Immersive Views
**Learning:** When building full-screen or immersive interfaces (like the e-book viewer) that support keyboard navigation for primary actions (e.g., arrow keys for page turning), users intuitively expect a complementary keyboard shortcut (like `Escape`) to exit or go back. Without it, keyboard-only users must disrupt their reading flow to manually tab through the interface to find a back button.
**Action:** Always pair primary keyboard navigation in immersive views with an intuitive "exit" or "back" shortcut (e.g., `Escape`), and explicitly document the shortcut in the UI (e.g., via tooltips or aria-labels).
## 2026-06-25 - [Contextual Placeholders]
**Learning:** Found that primary forms like the "Upload Book" form in `index.html` were using placeholders that simply duplicated the label text (e.g., `placeholder="Title"` for the Title field). This fails to provide any extra value and does not help the user understand the expected input format. Using contextual examples (e.g., `placeholder="e.g., The Great Gatsby"`) reduces cognitive load and clarifies expectations.
**Action:** When designing or updating forms, ensure that `placeholder` attributes provide clear, concrete examples of the expected input rather than just repeating the field label.
## 2026-06-25 - Contextual Placeholders in Bulletin Board
**Learning:** Found that the Bulletin Board form used generic placeholders like 'neighbor' and 'What\'s on the board?' that didn't provide specific value or guidance. Updating these to explicit examples like 'e.g., Jane (Neighbor)' reduces cognitive load and gives better direction.
**Action:** Always ensure that placeholder attributes provide clear, concrete examples of the expected input rather than just repeating the field label or asking a generic question.
## 2026-06-25 - [Full-Screen Loader Accessibility]
**Learning:** Found that the full-screen loading overlay in the e-reader viewer updated its text dynamically to display error messages (e.g., "Error loading book: ...") without an `aria-live` attribute. This causes screen readers to completely miss the error state change, leaving visually impaired users indefinitely waiting for a book that failed to load.
**Action:** When implementing transient loading overlays or viewer error states, always wrap the status text in an `aria-live="polite"` or `aria-live="assertive"` container so screen readers are correctly notified of the state changes.
## 2024-08-15 - [File Upload Auto-fill]
**Learning:** Users uploading e-books often have files already named in an "Author - Title.epub" format. Forcing them to manually retype this information into the Author and Title fields is repetitive and increases friction during the upload process.
**Action:** When providing file upload forms that require metadata, listen to the file input's `change` event and attempt to intelligently parse and auto-fill the metadata fields (e.g., removing extensions and splitting by delimiters) if they are currently empty.
## 2026-06-25 - [Dropdown Menu Accessibility]
**Learning:** Found that custom dropdown menus triggered by hamburger icons lacked keyboard accessibility for closing. Users navigating by keyboard expect the `Escape` key to close active menus, dialogs, and popups.
**Action:** Always add a global `keydown` event listener for the `Escape` key when implementing custom dropdowns or popups, ensuring it hides the content and accurately updates the `aria-expanded` state of the trigger button.

## 2026-06-25 - [Live Audio Status Accessibility]
**Learning:** Found that dynamic changes in 'Now Playing' and playback status updates (playing/paused) in the Audio Radio UI (`audio.html`) were silently updated in the DOM, leaving screen reader users unaware of track progressions or control states when they weren't actively focused on the audio element.
**Action:** Always wrap dynamic media status displays (like 'Now Playing' elements) in an `aria-live="polite"` container to ensure changes in track or playback status are automatically announced to screen readers.
## 2026-06-25 - [Expiring Status Accessibility]
**Learning:** Using only a color change (like adding a `soon` CSS class) to indicate an approaching expiration fails to provide context for colorblind users and screen readers.
**Action:** Always pair semantic color changes with descriptive `title` and `aria-label` attributes to ensure the state change is universally perceivable.
## 2026-09-24 - [File Upload Accept Attribute]
**Learning:** Found that the file upload input lacked an `accept` attribute, meaning users could accidentally select and upload unsupported files (like videos or images), leading to backend errors or wasted storage space.
**Action:** When implementing file upload inputs, always use the `accept` attribute with explicitly supported file extensions (e.g., `accept=".epub,.pdf,.mobi,.azw3,.txt,.cbz,.cbr"`) to natively filter the OS file picker, preventing invalid uploads and reducing user error.
## 2024-10-25 - [Status Indicator Accessibility]
**Learning:** When creating visual non-text status indicators (like colored dots), relying on color alone is not accessible for colorblind and screen reader users.
**Action:** Always provide a human-readable text description via `title` (for hover) and `aria-label` (for screen readers), and apply `role="status"` to ensure universal accessibility.
## 2026-09-29 - [Disabled Button Accessibility]
**Learning:** Found an accessibility issue pattern where vanilla JavaScript explicitly toggles buttons into a disabled state and updates the 'title' attribute to provide a visual tooltip (e.g., 'Uploading...'), but fails to pair it with an 'aria-label'. Screen readers often struggle to announce 'title' attributes reliably when an element is disabled (or at all), meaning visually impaired users are left without context about why an interaction failed or what process is loading.
**Action:** Always mirror dynamically updated 'title' attributes with identical 'aria-label' attributes, especially when handling loading or disabled states, to ensure screen reader compatibility.
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
## 2026-12-14 - [ARIA Live Regions for Transient States]
**Learning:** When implementing transient full-screen loading overlays or viewer error states, applying `aria-live` directly to an element that toggles its `display` property can cause screen readers to fail to announce the state change reliably because the element wasn't originally part of the accessibility tree when hidden.
**Action:** Always wrap dynamically updating status text (like loading indicators or error banners) in a permanent `aria-live="polite"` or `aria-live="assertive"` container so screen readers correctly detect and announce the visibility or text changes of child elements.

## $(date +%Y-%m-%d) - Responsive Layout for Mobile Devices
**Learning:** Always remove temporary verification scripts (e.g., ad-hoc Playwright scripts) and delete `__pycache__` directories before committing or requesting code reviews to prevent accidentally polluting the repository with binary or throwaway files.
**Action:** When implementing responsive design (flex-wrap and ordering), strictly contain all changes within standard `@media` queries and test both mobile and desktop screen sizes to ensure changes haven't leaked out of the block. I added vertical stacking and 48px touch targets for mobile under an `@media (max-width: 768px)` media query in `style.css`.

## 2026-12-14 - [Character Count Hint Updates on Persisted Inputs]
**Learning:** When a form submission preserves certain input values (like an author's name) for convenience via `localStorage`, blindly resetting all character count hints to zero (e.g., `0/24`) causes the UI to become immediately out-of-sync with the preserved input string.
**Action:** When resetting forms, check if any input values are intentionally preserved. If they are, recalculate and update their specific character count hints dynamically (e.g., `nameInput.value.length + '/24'`) rather than setting them to zero.

## 2026-12-15 - [Form Submission Success Feedback]
**Learning:** When submitting forms that reset their state (like the new post form in the Bulletin Board), silently clearing the inputs without explicit success feedback leaves users unsure if their action succeeded, even if the new item appears in a list below.
**Action:** Always provide immediate inline success feedback (e.g., '? Posted') within an `aria-live='polite'` container after a successful form submission, ensuring it clears after a consistent delay (e.g., 4000ms).

## 2026-12-16 - [Timeout Overlaps in Success Messaging]
**Learning:** Using generic `setTimeout` calls for transient UI messages (like "? Posted" or "Upload successful") without tracking the timeout IDs can lead to overlaps. When a user submits forms in quick succession, the timeout from the first submission will unexpectedly clear the message from the second submission before the intended delay.
**Action:** When using `setTimeout` to manage transient UI states, always store the timeout ID (e.g., in an object dictionary or state object) and call `clearTimeout()` before setting a new one, ensuring the state remains visible for the full duration of the latest action.
## 2026-12-16 - [Vanilla JS Loading States with Animated Ellipsis]
**Learning:** Adding loading states to vanilla HTML buttons (like 'POSTING...') can be done efficiently without heavy JavaScript or external dependencies.
**Action:** Utilize CSS keyframe animations to create an animated ellipsis (via `content` property on a pseudo-element like `.loading-dots::after`). This clarifies the active state visually while maintaining a lightweight UI.

## 2026-12-16 - [Standardized Async Button Loading States]
**Learning:** Hardcoded text dots (e.g., 'Uploading...') on async buttons provide poor visual feedback and create inconsistencies when some buttons use CSS animations (like '.loading-dots' in 'admin.html' and 'board.html').
**Action:** Reused the existing lightweight '.loading-dots' CSS keyframe animation across all async actions ('script.js', 'setup.html') by injecting it via 'innerHTML', creating a unified, delightful, and dependency-free visual rhythm for the UI. Ensure that the CSS animation itself is included on all pages that use it (e.g. by injecting it inline in admin.html and setup.html, which do not load the global style.css).
## 2026-12-16 - [Keyboard Focus States Alignment]
**Learning:** Found several inputs and textareas in `admin.html` and `board.html` using the generic `:focus` pseudo-class for custom border highlights instead of the more accessible `:focus-visible`. This overrides the global `:focus-visible` styles, resulting in unwanted focus rings for mouse users and inconsistent keyboard navigation styling.
**Action:** Always prefer `:focus-visible` over `:focus` when applying focus highlights to form inputs and interactive elements to ensure a clean experience for mouse users while preserving accessibility for keyboard users.
## 2023-10-25 - Standardizing Empty States
**Learning:** For consistency across the design system, `.empty-state` containers should use the `.file-notes` CSS class for secondary subtext/call-to-actions, rather than plain unstyled text or custom inline styles.
**Action:** When adding or updating empty states, ensure subtext (e.g., instructions on how to populate the list) is wrapped in `<span class="file-notes">`.
## 2026-10-03 - Added aria-live wrappers to dynamic status messages
**Learning:** Dynamic text updates (like upload progress) in this app's vanilla JS architecture were visually updating but hidden from screen readers because the text nodes weren't wrapped in permanent `aria-live` regions.
**Action:** Always wrap dynamic status, loading, and progress text containers with `aria-live="polite"` or `assertive` upon initial render to ensure screen readers capture the state changes.

## 2026-10-04 - [Invisible Inline Validation Errors]
**Learning:** When replacing native form validation tooltips with custom inline error text inside a container that is initially hidden (`display: none`), setting the text content and class alone is insufficient; the error remains invisible to the user.
**Action:** Always ensure you explicitly toggle the display property (e.g., `stat.style.display = 'block';`) when dynamically populating hidden error containers during form validation to ensure the user actually receives the feedback.

## 2026-10-06 - Mirroring aria-label for disabled loading buttons
**Learning:** Screen readers often fail to read standard `title` attributes on `<button disabled>` elements. When dynamically switching an active button to a disabled state with loading text (e.g. `POSTING...`), it is crucial to explicitly set a descriptive `aria-label` matching the `title`. Furthermore, when the button is re-enabled, these attributes must be fully removed (`removeAttribute()`) rather than just set to empty strings, to prevent stale or invisible text from being announced in its idle state.
**Action:** When adding transient loading states to disabled buttons, always mirror `title` to `aria-label` when disabling, and invoke `removeAttribute('title')` and `removeAttribute('aria-label')` when re-enabling.

## 2026-12-16 - [Redundant Attributes on Text Buttons]
**Learning:** Adding `title` or `aria-label` to buttons that already have clear, visible text content (e.g., 'Upload' or 'Enter Sleep Mode') is redundant and an accessibility anti-pattern. Native tooltips can obscure the UI, and screen readers will reliably announce the button text anyway.
**Action:** When working with textual buttons, avoid duplicating the text content into `title` or `aria-label` attributes. Rely on the text content itself. Ensure any dynamically added loading states correctly remove these attributes when the button is re-enabled using `removeAttribute()`.
## 2024-10-10 - Audio disabled button aria-label
**Learning:** The 'Add to Queue' and 'Remove' buttons in the audio interface had a mismatched `title` and `aria-label` during their disabled (loading) state. This creates confusion for screen readers since the `aria-label` remained static.
**Action:** Always ensure `aria-label` accurately mirrors the dynamic visual text or `title` of a button, especially when transitioning into loading states.
