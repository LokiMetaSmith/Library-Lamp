## 2024-05-24 - Dynamic List Item Buttons ARIA Labels
**Learning:** When buttons like "Delete" or "Transfer" exist in dynamic Vue lists (v-for), screen readers announce generic identical names for all list items. Using dynamic :aria-label bindings with list item properties (file.title) provides essential context.
**Action:** Always bind :aria-label to list item specific data when buttons perform actions on dynamically generated list entries.
## 2024-07-02 - Audio Player Buttons ARIA Labels
**Learning:** Icon-only or context-dependent buttons within dynamic lists and media players (like "Remove", "Add to Queue", or "Skip Track") can lack clear context for screen reader users if relying solely on generic text or visual placement. Using Vue's `:aria-label` binding allows interpolating current state or list item properties (like the current track name or list item title) to create highly descriptive, accessible labels.
**Action:** Always add dynamic `:aria-label` bindings that incorporate specific state variables (e.g., `currentTrack`, `file.title`) when implementing controls for media players or dynamic item lists.
