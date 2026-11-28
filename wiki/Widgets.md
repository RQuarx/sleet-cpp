# Widgets and modifiers

The factories are declared in `sleet/widgets.hh`. The four primitives cover
custom composition:

```cpp
box();      // single-child or freeform container
row();      // horizontal stack
column();   // vertical stack
label("text");
```

`floating_box(anchor, stacking)` is a box that lives outside its parent's flow.
It anchors to a parent edge or corner and acts as the root of a popup or
overlay subtree; see [[Layout]] for anchors and paint order.

Built-in widgets are thin compositions of those primitives:

| Factory | Description |
| --- | --- |
| `button(text, action)` | Text button; also accepts arbitrary `node_data` content |
| `text_input(value, action, hint)` | Single-line editable field |
| `text_area(value, action, hint)` | Multiline wrapped editor; Ctrl/Cmd+Enter submits |
| `combo_box(value, options, action, hint)` | Inline drop-down list |
| `checkbox(text, checked, action)` | Labeled checkbox |
| `toggle(text, on, action)` | Sliding on/off switch |
| `radio_button(text, selected, action)` | Labeled radio button |
| `slider(value, action)` | Horizontal slider with a normalized value |
| `progress_bar(value, height)` | Horizontal progress indicator |
| `list_view()` | Scrollable, focus-grouped list surface |
| `list_item(text, action, selected)` | Selectable list row |
| `table_view()` | Scrollable table surface |
| `table_header()`, `table_row(action, selected)` | Header and selectable data rows |
| `table_cell(text, weight)`, `table_header_cell(text, weight)` | Weighted table columns |
| `tree_view()` | Scrollable, focus-grouped tree surface |
| `tree_item(text, action, options)` | Selectable row with depth and disclosure state |
| `separator()` | One-pixel horizontal rule |
| `spacer()` | Flexible blank area |
| `badge("text")` | Compact accent-colored label |
| `menu_item(text, action, selected)` | Menu row with a selection marker |
| `menu()` | Floating themed panel for a popup menu |

Combo options derive their generated ids from their labels, so filtering or
reordering a list keeps each option's identity. When the labels themselves can
change, pass the overload with explicit ids:

```cpp
combo_box(current.color,
          { "Red", "Green", "Blue" },
          { widget_id { "color-red" }, widget_id { "color-green" },
            widget_id { "color-blue" } },
          "color");
```

```cpp
column()[text_input(current.name, "name", "Type your name..."),
         text_area(current.notes, "notes", "Write a few lines..."),
         combo_box(current.color, { "Red", "Green", "Blue" }, "color", "Pick one"),
         row()[checkbox("Details", current.details, "details"),
               toggle("Notifications", current.notifications, "notifications")],
         slider(current.volume, "volume"),
         progress_bar(current.volume),
         row()[button("-", "decrement"), button("+", "increment", button_variant::primary)]];
```

Buttons accept four visual weights through `button_variant`: `standard`,
`primary`, `ghost`, and `danger`. The `button(node_data, action, ...)`
overload allows custom content such as a row of labels and badges.

## Lists, tables, and trees

Lists and tables are scrollable, focus-grouped surfaces. The application owns
the data and the selected value; the runtime owns focus, scrolling, and
keyboard navigation. Rebuild the rows from state each frame and mark the
selected row with `selected(true)`:

```cpp
auto rows = list_view() | width(260.F) | height(220.F);
for (const auto &item : current.items)
    rows = std::move(rows)[list_item(item.name, item.action,
                                     item.action == current.selected)];
```

Rows are focusable controls. Tab enters the list once, Up/Down move focus,
Home/End jump to the ends, and PageUp/PageDown move by a viewport. Enter, Space,
or a click emits `event::kind::activate` with the row's action. The runtime does
not change `selected` itself, so `on_update` decides what activation means and
the next view paints the new selection. `list_item(node_data, action, selected)`
accepts arbitrary row content.

A table uses the same row behavior and adds weighted columns:

```cpp
auto table = table_view() | height(220.F);
table = std::move(table)[table_header()[table_header_cell("Name", 2.F),
                                         table_header_cell("Size", 1.F)]];
for (const auto &file : current.files)
{
    auto row = table_row(file.action, file.action == current.selected);
    row = std::move(row)[table_cell(file.name, 2.F), table_cell(file.size, 1.F)];
    table = std::move(table)[std::move(row)];
}
```

Header and data cells must use matching weights. The table surface paints a
one-pixel gap between rows, so header and row backgrounds read as a grid.

Trees keep their rows as flat siblings and describe the hierarchy with `depth`.
Include only the rows that are currently visible; collapsing a parent simply
omits its descendants:

```cpp
auto tree = tree_view() | height(240.F);
tree = std::move(tree)[
    tree_item("Documents", "docs") | depth(0) | has_children()
        | expanded(current.docs_open) | on_toggle("toggle_docs")];

if (current.docs_open)
    tree = std::move(tree)[tree_item("Guide", "guide") | depth(1),
                           tree_item("API", "api") | depth(1)];
```

Right expands a collapsed row or moves to its first child; Left collapses an
expanded row or moves to its parent. Clicking the disclosure chevron emits the
toggle action instead of activating the row. `tree_item` also accepts a
`tree_item_options` value with `depth`, `expanded`, `has_children`, and
`toggle_action` fields.

Row ids are derived from their actions, so reordering or filtering rows keeps
focus and scroll state attached to the right item. When several rows share an
action, give each one an explicit `key(...)`.

## Modifiers

Modifiers are applied with `|`:

```cpp
box()[label("Hello")]
    | padding(12.F)
    | background(0x2F6FEB_rgb)
    | on_click("hello");
```

| Modifier | Effect |
| --- | --- |
| `id("name")`, `key("name")` | Explicit stable widget id |
| `width`, `height`, `size` | Fixed or `sizing` values |
| `hexpand`, `vexpand` | `sizing::fill` on one axis |
| `flex(weight)` | Relative share when growing or filling |
| `margin`, `padding` | `math::edge_insets` values |
| `gap(px)` | Space between row/column children |
| `halign`, `valign` | Child alignment inside a box |
| `align_self(align)` | Override alignment for one child |
| `justify_content` | Main-axis distribution in a row/column |
| `visible(bool)` | Hide a subtree |
| `floating(bool)`, `anchor_at(anchor_point)`, `placement(x, y)`, `z_index(n)`, `stacking(n)` | Overlay positioning outside normal flow |
| `clip(bool)`, `clip_children(bool)` | Clip descendants to this node's bounds |
| `scrollable(bool)` | Wheel-scroll a clipped viewport |
| `scrollbar(bool)` | Show or hide the proportional scrollbar on a scrollable node |
| `ensure_visible(bool)` | Scroll ancestors to reveal the node when it gains focus |
| `disabled(bool)` | Remove a subtree from focus and input; disabled widgets are not pointer targets |
| `text_size`, `text_weight`, `text_role`, `text_color`, `text_decoration` | Text styling |
| `text_wrap(mode)` | `no_wrap`, `wrap`, or `wrap_anywhere` for constrained text |
| `text_align(alignment)` | `left`, `center`, `right`, or `justify` text without changing child layout |
| `background`, `foreground`, `border`, `border_width` | Visual styling |
| `radius(px)` | Override the theme's corner radius |
| `on_click("action")` | Attach an action name for event mapping |
| `on_shortcut(chord, "action")` | Register a keyboard shortcut active while this subtree is visible |
| `focusable()` | Include the widget in the Tab focus order |
| `autofocus()` | Focus this widget or its first focusable descendant when nothing valid is focused |
| `focus_group(bool)` | Treat the subtree as one Tab stop with arrow-key navigation |
| `tone_of(tone)`, `variant(button_variant)` | Resolve semantic colors from the theme |
| `background_of(tone)`, `border_of(tone)` | Fill or outline using a theme tone |
| `control(kind)`, `checked(bool)`, `control_value(float)`, `placeholder(text)` | Control metadata; built-in widgets set these for you |
| `selected(bool)` | Paint the theme selection color behind a row or other widget |
| `depth(int)`, `expanded(bool)`, `has_children(bool)`, `on_toggle("action")` | Tree row indentation, disclosure state, and toggle action |
| `transition(seconds, curve)`, `no_transition()` | Ease hover, press, and focus feedback; `no_transition()` opts out |
| `tooltip("text")` | Show a tooltip after the pointer rests on the node or a descendant |
| `backdrop(bool)` | Fill the parent content box and paint the theme scrim |
| `focus_scope(bool)` | Keep Tab traversal inside this subtree once focus enters it |
| `modal(cancel_action)`, `on_cancel("action")` | Block input outside the subtree and map Escape or a scrim click to an action |

## Motion

Interactive widgets ease hover, press, and focus feedback over 120 ms by
default. Override the duration or curve per node:

```cpp
button("Save", "save") | transition(0.2F, easing::ease_in_out);
box()[...] | no_transition();
```

`easing` provides `linear`, `ease_in`, `ease_out`, `ease_in_out`, and
`ease_out_back`. The runtime animates the scalar state and the renderer blends
colors and focus borders by the eased amount instead of switching instantly.
Transitions snap when no progress map is supplied, so layout tests and static
rendering stay deterministic. Call `application::set_reduced_motion()` to
disable easing for accessibility; transitions and smooth scrolling then snap to
their targets.

Nodes without an explicit id receive a generated id from their kind, name, call
site, and per-parent occurrence. The call site is part of the key, so inserting
an unrelated sibling does not renumber existing widgets. Combo options add
their label, and list or tree rows add their action, so reordering those
collections does not detach focus or scroll state. See [[Layout]] for overlay
and scrolling rules.

## Shortcuts and focus

Application-wide shortcuts are registered on the application; widget-scoped
shortcuts travel with the view:

```cpp
auto panel = column()[...] | on_shortcut("Ctrl+S", "save");
app.on_shortcut("Ctrl+K", "command_palette");
```

`on_shortcut` accepts a text chord such as `"Ctrl+Shift+S"`, `"Cmd+Enter"`, or
`"F5"`, a `key_chord`, or a key code plus modifier mask. Modifiers must match
exactly, Caps Lock and Num Lock are ignored, and repeats are ignored unless the
chord sets `repeat`. A match emits a `key_down` event whose `action` is the
bound action.

View-scoped bindings only fire while their subtree is visible, and the deepest
matching binding wins over shallower and application bindings. Built-in
editing and control keys take precedence, and while a text input is focused,
shortcuts without Ctrl, Alt, or Super are skipped so typing is never stolen;
function keys still work.

`autofocus()` requests focus when no valid widget is focused, including when
its modal opens. `focus_group()` collapses a subtree into one Tab stop: Left/Up
and Right/Down move between members, Home and End jump to the ends, and Tab
remembers the member that was active last. Text inputs, sliders, and combo
boxes keep their own arrow-key behavior inside a group.

## Overlays

Tooltips attach with `tooltip("text")`. The runtime shows one after the pointer
rests for 550 ms on the node or a descendant that owns the tooltip, orients it
above or below the anchor, and hides it on movement, clicks, or scrolling.

A popup menu is a floating `menu()` panel, usually built from `menu_item`
rows. Dismiss it by making the panel modal and toggling the state from the
mapped action:

```cpp
auto panel = menu() | placement(0.F, 40.F) | modal("menu_close");
panel      = panel[menu_item("Duplicate", "duplicate"),
                    menu_item("Archive", "archive", true), separator(),
                    menu_item("Delete", "delete")];
trigger    = trigger[std::move(panel)];
```

`modal(action)` does three things:

- Blocks hit testing outside the modal subtree. Clicks that land on the modal
  surface itself, including its padding, emit `action`; clicks on descendants
  keep their own actions.
- Traps focus. Tab and Shift+Tab cycle within the subtree, and the previously
  focused widget regains focus when the modal disappears.
- Maps Escape to `on_cancel("action")`, or to the `modal(action)` argument when
  no explicit cancel action is set.

`focus_scope()` is the lighter alternative: when focus enters the subtree, Tab
stays inside it, but outside pointer input is not blocked.

A dialog combines these pieces. Put a `backdrop()` floating box over the
content, mark it modal, and center a panel inside it:

```cpp
auto overlay = floating_box(anchor_point::center, 400) | backdrop()
             | modal("dialog_cancel");
overlay = overlay[column()[label("Save changes?"),
                           row()[button("Cancel", "dialog_cancel"),
                                 button("Save", "dialog_save")]]];
root = root[std::move(overlay)];
```

Toast notifications are runtime-managed rather than widget trees. Capture the
application in `on_update` and call `notify(text, seconds)`; toasts fade and
slide in, stack bottom-right, cap at four, and expire after the given duration
(default 3 seconds). `notify` is not thread-safe, so call it from the event-loop
thread or an update callback. Use `set_reduced_motion()` to snap instead of
animating.

`math::edge_insets` uses CSS order: top, right, bottom, left. The two-argument
constructor is `(vertical, horizontal)`.

See [[Controls]] for the events these controls emit and [[Layout]] for how
sizing and composition resolve.
