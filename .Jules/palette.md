## 2024-05-24 - Dynamic List Item Buttons ARIA Labels
**Learning:** When buttons like "Delete" or "Transfer" exist in dynamic Vue lists (v-for), screen readers announce generic identical names for all list items. Using dynamic :aria-label bindings with list item properties (file.title) provides essential context.
**Action:** Always bind :aria-label to list item specific data when buttons perform actions on dynamically generated list entries.
