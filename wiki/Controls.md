# Controls

Control widgets are ordinary node trees with a small amount of runtime state.
The application owns the values; the runtime owns focus, the caret and
selection, open combo boxes, and slider dragging. Widget ids are path-based, so
that state survives ordinary view rebuilds.

## Events

Buttons, checkboxes, toggles, and radio buttons report their `action` in a
`mouse_up` event and in an `activate` event when activated with Enter or Space.

| Kind | Source | Payload |
| --- | --- | --- |
| `text_edit` | Text input | `text` contains the new value |
| `submit` | Text input | `text` contains the submitted value; emitted on Enter |
| `selection_changed` | Combo box | `text` contains the chosen option |
| `slider_changed` | Slider | `value` contains the normalized value in `[0, 1]` |
| `activate` | Button, checkbox, toggle, radio button | `action` contains the widget action |

A text input reports `text_edit` while editing and `submit` on Enter. A combo
box reports `selection_changed` with the option text. The runtime sets the
control event's `action` from the `on_click` action supplied to the factory.

```cpp
case event::kind::text_edit:
    if (input.action == "name") state.name = input.text;
    break;

case event::kind::selection_changed:
    if (input.action == "color") state.color = input.text;
    break;

case event::kind::slider_changed:
    if (input.action == "volume") state.volume = input.value;
    break;

case event::kind::activate:
    if (input.action == "details") state.show_details = !state.show_details;
    break;
```

## Keyboard

Tab and Shift+Tab move focus through visible, focusable widgets. A focused text
input supports typing, arrows, Home and End, Shift-selection, Ctrl+A, Ctrl+C,
Ctrl+X, and Ctrl+V. Escape clears text focus. Focused buttons and selection
controls activate with Enter or Space; focused sliders change value with the
arrow keys.

The pointer behavior is symmetric: a mouse release activates an action only
when the press began on the same action widget, so dragging off a button
cancels the click. Checkbox and radio indicators are visual parts of their
parent widget, so pressing either the indicator or the label toggles the
control. Clicking outside an open combo closes it, and clicking a slider starts
a drag that continues until the left button is released.

See [[Application-Runtime]] for how events reach `on_event` and
[[Rendering-and-Themes]] for focus and selection drawing.
