# Layout model

Layout is a separate, pure pass over a finalized widget tree. It does not touch
the renderer, which is why layout tests can use a deterministic measurer
callback instead of a window.

```cpp
auto tree = ...;                                  // widget::node_data
auto result = tree.finalize();                    // assign ids and parents
if (!result) { /* report result.error() */ }

auto layout = sleet::widget::calculate_layout(
    tree,
    [&](std::string_view text, const sleet::widget::font_spec &font,
        std::optional<float> width_limit)
    { return context.measure_text(text, font, width_limit); },
    { 800.F, 600.F });
```

## Sizing

- `sizing::fit()` uses the intrinsic size (the default).
- `sizing::grow()` shares the remaining space on the parent's main axis
  according to `flex`. On the cross axis it keeps its intrinsic size.
- `sizing::fill()` stretches across the parent's cross axis and shares the main
  axis like `grow`. Inside a `box`, both modes stretch on both axes.
- `sizing::fixed(px)` uses an explicit size.

Fixed sizes include padding. `flex` controls the relative share of positive
free space. If free space is negative, flexible children shrink by the same
proportion. `hexpand` and `vexpand` use `sizing::fill`, so they stretch the
requested axis when the parent has a definite size, whether that axis is the
main or cross axis.

## Text

A text node's intrinsic width is measured without wrapping. When its resolved
width is constrained — by a fixed-width box or column, or because the node
itself is `fill` or `grow` — the text is re-measured and wrapped at that width.
A fit-width item on a row's main axis keeps its intrinsic width. `txt::wrap`
treats any supplied width as a limit and clamps it to zero; passing
`std::nullopt` is the only way to get unwrapped text. Production code passes
`render_context::measure_text` as the `text_measurer`, and tests can pass a
deterministic callback instead.

## Boxes and stacks

- A `box` aligns its children inside its content box with `halign` and
  `valign`. A child can override that with `align_self`.
- A `row` or `column` places children along its main axis and aligns them on
  the cross axis. `justify_content` supports `start`, `center`, `end`, and
  `between`.

## Identity

`finalize()` assigns stable ids to nodes without an explicit id. A generated id
combines the node kind, its name, its source location, and an occurrence number
within its parent, so inserting or reordering an unrelated sibling does not
renumber existing widgets. Combo options also include their label in the key,
which keeps identity attached to the option value rather than its index.

Use `id("name")` or `key("name")` when an id must be fully explicit, especially
inside loops. Explicit ids are checked for duplicates, and `finalize()` fails
before layout when two nodes collide. Widget ids are the keys used by
`layout_result::find`, hit testing, and hover/press state; see [[Controls]] for
how the runtime uses them.

## Floating overlays

`floating()` takes a node out of its parent's flow. Its size is still measured,
but it consumes no space and does not affect the parent's intrinsic size.
`anchor_at(anchor_point)` selects the parent content-box edge or corner to
position against, `placement(x, y)` adds a local offset, and `z_index(n)` (or
its alias `stacking(n)`) controls paint order:

```cpp
auto menu = floating_box(anchor_point::top_left, 100)[...]
          | placement(0.F, 38.F);
```

The anchors are `top_left`, `top_center`, `top_right`, `center_left`, `center`,
`center_right`, `bottom_left`, `bottom_center`, and `bottom_right`. Margins
inset a floating node from the anchored edges; centered axes ignore margins so
the node stays centered.

`floating_box(anchor, stacking)` is a `box_node` with `floating`, `anchor_at`,
and `z_index` already applied. It is the root of its overlay subtree, so its
children use ordinary row, column, and box layout. A floating node used as the
layout root is laid out like any other root and is not collected as an overlay.

Floating nodes are painted after all in-flow content, in ascending `z_index`
order. They are hit-tested first, in reverse paint order, so a popup receives
pointer input before the content underneath it. `layout_result::floating` lists
the visible overlays in paint order as `{ id, z_index }` entries; storing ids
instead of pointers lets a layout safely outlive its tree.

## Scrolling and clipping

`clip_children()` clips descendants to the node's border box. Clipping applies
to both rendering and hit testing: a child outside every clipping ancestor's
bounds is neither drawn nor clickable.

`scrollable()` makes a node respond to wheel events. Each wheel step requests
48 logical pixels; the runtime moves part of the distance immediately and eases
the rest over a few frames. The offset is clamped to the measured content bounds
and applied after layout by translating the subtree. Nested scroll containers
are resolved bottom-up, so an outer viewport also moves already-panned inner
content. The layout pass itself is always unscrolled; scrolling is runtime
state, like focus and text selection.

`scrollbar()` controls the proportional thumb drawn along the right or bottom
edge when content overflows. `ensure_visible()` scrolls the scrollable
ancestors when a widget gains focus; it is enabled by default and can be turned
off per widget. `application::set_reduced_motion()` makes wheel and focus
scrolling snap instead of easing.

A typical scroll viewport combines both modifiers:

```cpp
column()[rows...] | height(240.F) | clip() | scrollable() | scrollbar();
```

See [[Application-Runtime]] for wheel dispatch and [[Rendering-and-Themes]] for
the painting side of clipping and overlays.
