# Rendering and themes

`render_context` owns the font cache, the text-texture cache, the theme, and the
active hover, press, focus, and selection state.

```cpp
sleet::render_context context { renderer, sleet::widget::theme::default_dark() };
context.set_pixel_scale(display_scale);       // physical pixels per layout point
context.set_viewport({ width, height });
context.set_hovered(hovered_id);
context.set_pressed(pressed_id);
context.set_focused(focused_id);
context.set_text_selection(focused_id, sleet::widget::text_selection { .caret = 3,
                                                                       .anchor = 0 });
context.set_preedit(focused_id, "composition", 4);

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

## High-DPI rendering

Layout, pointer coordinates, and theme sizes are in logical points. The renderer
converts to physical pixels using `set_pixel_scale`, which the application
derives from the renderer output size and the window point size. Fonts are
rasterized at the physical scale, text is snapped to physical pixels, and
shapes are drawn in physical coordinates. The runtime does not use SDL's logical
presentation, so layout never needs to compensate for a hidden scale factor.
Changing the scale clears the text cache.

## Drawing

Rounded surfaces, borders, circles, checkmarks, and badges are drawn with
`SDL_RenderGeometry` using alpha-feathered contours. That gives antialiased
rounded corners without a separate mask or texture; square fills use
`SDL_RenderFillRect`. `theme::radius` supplies the default control radius and
`theme::small_radius` covers indicators such as checkboxes; `radius(px)`
overrides either per widget.

Focused widgets draw a two-pixel focus border. Text inputs render their
placeholder, caret, selection highlight, IME preedit text, and horizontal
scroll. Checkbox and radio indicators, combo headers and popup surfaces, and
progress fills resolve their colors from the control kind and semantic tone.
Sliders are drawn from their control value as a rounded track, an accent fill,
and a centered thumb; focus rings the thumb instead of the whole row, and
disabled sliders mute all three parts.
Hover, press, and focus amounts are eased by the runtime, and the renderer
blends backgrounds, borders, and focus rings by the current progress instead of
switching states. Scrollable viewports draw a six-pixel proportional scrollbar
using the theme's `scrollbar_track` and `scrollbar_thumb` colors.
Hover and press colors apply to action widgets and propagate to their
descendants, so a label inside a button inherits the button's state foreground
unless it sets an explicit color. Disabled widgets use the theme's `disabled`
and `disabled_border` colors. Explicit widget colors take precedence over theme
defaults.

Clipped subtrees are intersected with every `clip_children()` ancestor. Floating
overlays are painted after all in-flow content in ascending `z_index` order.
Text textures are cached by font, text, and color; the cache is evicted
least-recently-used once it reaches its fixed limit, so long sessions do not
grow without bound.

## Themes and fonts

`widget::theme` provides base, hover, pressed, field, and disabled colors, plus
border, focus, accent, danger, caret, selection, and muted colors, the `radius`
and `small_radius` tokens, and the font roles. `theme::default_dark()` and
`theme::default_light()` discover common system fonts. A custom theme can set
only the body face, and missing roles inherit it.

The font roles are:

- `body`
- `heading`
- `caption`
- `code`

`font_config::fallbacks` lists extra faces tried in order when the selected role
does not cover a text run. An application can switch themes with
`application::set_theme`; the live render context picks up the change
immediately and rebuilds its text cache.

## Known limitations

There is no accessibility tree or screen-reader bridge, and text is not
reordered for bidirectional scripts. Font fallback chooses one face per text
run rather than shaping per glyph, and complex-script shaping is limited to what
SDL_ttf provides. These are explicit boundaries of the current renderer, not
configuration switches.
