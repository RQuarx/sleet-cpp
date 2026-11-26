# Widgets and modifiers

The factories are declared in `sleet/widgets.hh`. The four primitives cover
custom composition:

```cpp
box();      // single-child or freeform container
row();      // horizontal stack
column();   // vertical stack
label("text");
```

Built-in widgets are thin compositions of those primitives:

| Factory | Description |
| --- | --- |
| `button(text, action)` | Text button; also accepts arbitrary `node_data` content |
| `text_input(value, action, hint)` | Single-line editable field |
| `combo_box(value, options, action, hint)` | Inline drop-down list |
| `checkbox(text, checked, action)` | Labeled checkbox |
| `toggle(text, on, action)` | Sliding on/off switch |
| `radio_button(text, selected, action)` | Labeled radio button |
| `slider(value, action)` | Horizontal slider with a normalized value |
| `progress_bar(value, height)` | Horizontal progress indicator |
| `separator()` | One-pixel horizontal rule |
| `spacer()` | Flexible blank area |
| `badge("text")` | Compact accent-colored label |

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
| `floating(bool)`, `placement(x, y)`, `z_index(n)` | Overlay positioning outside normal flow |
| `clip(bool)`, `clip_children(bool)` | Clip descendants to this node's bounds |
| `scrollable(bool)` | Wheel-scroll a clipped viewport |
| `disabled(bool)` | Remove a subtree from focus and input; disabled widgets are not pointer targets |
| `text_size`, `text_weight`, `text_role`, `text_color`, `text_decoration` | Text styling |
| `background`, `foreground`, `border`, `border_width` | Visual styling |
| `radius(px)` | Override the theme's corner radius |
| `on_click("action")` | Attach an action name for event mapping |
| `focusable()` | Include the widget in the Tab focus order |
| `tone_of(tone)`, `variant(button_variant)` | Resolve semantic colors from the theme |
| `control(kind)`, `checked(bool)`, `control_value(float)`, `placeholder(text)` | Control metadata; built-in widgets set these for you |

Nodes without an explicit id receive a generated id from their kind, name, call
site, and per-parent occurrence. The call site is part of the key, so inserting
an unrelated sibling does not renumber existing widgets. See [[Layout]] for
overlay and scrolling rules.

`math::edge_insets` uses CSS order: top, right, bottom, left. The two-argument
constructor is `(vertical, horizontal)`.

See [[Controls]] for the events these controls emit and [[Layout]] for how
sizing and composition resolve.
