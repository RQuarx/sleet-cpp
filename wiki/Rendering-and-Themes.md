# Rendering and themes

`render_context` owns the font cache, the text-texture cache, the theme, and the
active hover, press, focus, and selection state.

```cpp
sleet::render_context context { renderer, sleet::widget::theme::default_dark() };
context.set_viewport({ width, height });
context.set_hovered(hovered_id);
context.set_pressed(pressed_id);
context.set_focused(focused_id);
context.set_text_selection(focused_id, sleet::widget::text_selection { .caret = 3,
                                                                       .anchor = 0 });

auto measure = [&](std::string_view text, const sleet::widget::font_spec &font,
                   std::optional<float> width_limit)
{ return context.measure_text(text, font, width_limit); };

auto layout = sleet::widget::calculate_layout(tree, measure, { width, height });
auto rendered = context.render(tree, *layout);
```

`render_context` requires SDL3_ttf to be initialized. `application` handles
that and the SDL runtime automatically. Rendering returns `kei::result<>`, so
draw failures come back as errors rather than exceptions. The constructor
throws `kei::error` when the renderer is null or no usable body font is
configured. If a new body font cannot be opened, `set_theme` restores the
previous theme and throws.

Focused widgets draw a two-pixel focus border. Text inputs render their
placeholder, caret, selection highlight, and horizontal scroll. Checkbox and
radio indicators, combo headers, slider tracks, and progress fills resolve
their colors from the control kind and semantic tone. Hover and press colors
apply to action widgets and propagate to their descendants, so a label inside a
button inherits the button's state foreground unless it sets an explicit color.
Explicit widget colors take precedence over theme defaults.

## Themes and fonts

`widget::theme` provides base, hover, pressed, and field colors, plus border,
focus, accent, danger, caret, selection, and muted colors and the font roles.
`theme::default_dark()` and `theme::default_light()` discover common system
fonts. A custom theme can set only the body face, and missing roles inherit it.

The font roles are:

- `body`
- `heading`
- `caption`
- `code`

An application can switch themes with `application::set_theme`. The runtime
rebuilds the text cache when the theme changes.
