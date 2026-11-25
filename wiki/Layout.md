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

`finalize()` assigns stable path-based ids to nodes without an explicit id.
Explicit ids are checked for duplicates. Widget ids are the keys used by
`layout_result::find`, hit testing, and hover/press state; see [[Controls]] for
how the runtime uses them.
