# Controls

Control widgets are ordinary node trees with a small amount of runtime state.
The application owns the values; the runtime owns focus, the caret and
selection, IME composition, undo/redo history, open combo boxes, scroll
offsets, and slider dragging. Widget ids are stable across ordinary view
rebuilds, so that state survives a new tree each frame.

## Events

Buttons, checkboxes, toggles, and radio buttons report their `action` in a
`mouse_up` event and in an `activate` event when activated with Enter or Space.

| Kind | Source | Payload |
| --- | --- | --- |
| `text_edit` | Text input | `text` contains the new value |
| `submit` | Text input | `text` contains the submitted value; emitted on Enter (Ctrl/Cmd+Enter for a text area) |
| `selection_changed` | Combo box | `text` contains the chosen option |
| `slider_changed` | Slider | `value` contains the normalized value in `[0, 1]` |
| `activate` | Button, checkbox, toggle, radio button, list/table/tree row | `action` contains the widget action; rows also put their label in `text` |

A text input reports `text_edit` while editing and `submit` on Enter. A combo
box reports `selection_changed` with the option text. The runtime sets the
control event's `action` from the `on_click` action supplied to the factory.
`text_edit` is also emitted after undo, redo, paste, cut, and IME commit, so
the application state always reflects what the field shows.

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

Tab and Shift+Tab move focus through visible, focusable widgets. Disabled
subtrees are skipped entirely.

A focused text input supports:

- typing and IME composition; the composition is drawn as preedit text and
  replaced, not appended, when the platform commits it;
- grapheme-aware Left/Right, Home/End, and Shift-selection;
- word movement and word deletion with Ctrl or Alt plus Left/Right/Backspace/
  Delete;
- Ctrl/Cmd+A, C, X, and V for select-all, copy, cut, and paste;
- Ctrl/Cmd+Z for undo and Ctrl/Cmd+Shift+Z or Ctrl/Cmd+Y for redo;
- double-click to select a word and triple-click to select the whole value;
- Enter to submit and Escape to cancel a composition or clear focus.

`text_area(value, action, hint)` uses the same editing behavior but is
multiline. Enter inserts a newline, Ctrl/Cmd+Enter submits, and Up/Down move
between visual lines while preserving the preferred horizontal position.
Home/End go to the start or end of the current visual line; Ctrl/Cmd+Home and
Ctrl/Cmd+End go to the start or end of the value. The field wraps by default,
scrolls vertically to keep the caret visible, and accepts multiline paste with
CRLF and CR normalized to LF.

Focused buttons and selection controls activate with Enter or Space; focused
sliders change value with the arrow keys. A focused combo box opens with Enter,
Space, or an arrow key. Up/Down and Home/End move through its options, Enter or
Space chooses one, and Escape or Tab closes the list. Only one combo box is open
at a time; opening another closes the first, and Tab returns focus to the header
before continuing traversal.

List, table, and tree rows are focusable controls inside a focus-grouped
surface. Tab enters the surface once. Up/Down move focus, Home/End jump to the
ends, and PageUp/PageDown move by one viewport. Enter, Space, or a click emits
`activate` with the row action. The runtime does not change selection itself;
rebuild the view with `selected(true)` on the row that should be selected.

Tree rows also use Left/Right. Right expands a collapsed row by emitting its
toggle action, then moves to the first child once the row is expanded. Left
collapses an expanded row, or moves to the parent when the row is a leaf or
already collapsed. Clicking the disclosure chevron emits the toggle action
instead of activating the row. Rows are flat siblings whose `depth` field
drives indentation and parent/child navigation; omit collapsed descendants from
the view.

A `focus_group()` subtree is one Tab stop. Arrow keys move focus between its
focusable members, Home and End jump to the first and last, and Tab re-enters at
the member that was focused last. Text inputs, sliders, combo boxes, and
list/tree rows keep their own arrow-key handling and are not navigated by the
group while focused.

## Pointer and disabled state

The pointer behavior is symmetric: a mouse release activates an action only
when the press began on the same action widget, so dragging off a button
cancels the click. Releasing outside the window, losing focus, or a view
rebuild that removes the pressed widget clears the drag state. Checkbox and
radio indicators are visual parts of their parent widget, so pressing either
the indicator or the label toggles the control. List, table, and tree rows
focus and emit `activate` on press; pressing a tree row's disclosure area emits
its toggle action instead. Clicking outside an open combo closes it, and
clicking a slider starts a drag that continues until the left button is
released. The wheel scrolls the nearest `scrollable()` ancestor.

`disabled()` removes a subtree from the Tab order and keyboard input, and
prevents its widgets from becoming pointer targets. It paints the subtree with
the theme's disabled colors. Disabled combo options are skipped by arrow
navigation.

## Known limitations

The runtime does not expose an accessibility tree or screen-reader semantics,
and text layout does not implement bidirectional reordering or full complex
script shaping. Those features need platform integration and a shaping engine
beyond SDL_ttf; treat them as future work rather than supported behavior.

See [[Application-Runtime]] for how events reach `on_event` and
[[Rendering-and-Themes]] for focus and selection drawing.
