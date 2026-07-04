## 2024-07-04 - [Board Category Accessibility]
**Learning:** When using custom button groups instead of standard `<select>` or radio buttons for form inputs (like the Category selector on the board), screen readers lose the context that the label applies to the entire group of buttons.
**Action:** Always wrap custom button groups in an element with `role="group"` and use `aria-labelledby` pointing to the ID of the visual label to maintain accessibility context.
