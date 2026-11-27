# Sleet-cpp

Sleet is a small C++23 UI framework modeled on the
[Elm architecture](https://guide.elm-lang.org/architecture/). Widgets are plain
values, layout is a pure calculation, and rendering is a thin layer over SDL3.

There are four primitive nodes — `box`, `row`, `column`, and `label` — and the
framework ships with buttons, text inputs, combo boxes, checkboxes, toggles,
radio buttons, sliders, progress bars, separators, spacers, and badges. All text
is UTF-8, line breaking follows Unicode rules, and the application runtime owns
the window, renderer, and event loop.

## Features

- Widget trees are ordinary values. Modifiers compose with `operator|`.
- Flexbox-style layout with `fit`, `grow`, `fill`, and `fixed` sizing, plus
  margins, padding, gaps, alignment, justification, and per-child flex weights.
- Floating overlays (`floating_box`, `anchor_at`, `placement`, `z_index`),
  clipping, wheel-scrollable viewports with proportional scrollbars, smooth
  wheel easing, and focus-reveal scrolling.
- Eased hover, press, and focus transitions with configurable curves and a
  reduced-motion mode.
- Stable generated widget ids derived from the call site, with explicit `id` or
  `key` overrides.
- Text is measured at its intrinsic size and wrapped using Unicode line-break
  rules, with `no_wrap` and `wrap_anywhere` modes plus left, center, right, and
  justified alignment. Editing is grapheme-aware and supports word movement,
  selection, undo/redo, and IME composition with native candidate-window
  placement.
- Rendering uses SDL3 in physical pixels for high-DPI windows, with
  antialiased rounded primitives, cached fonts and text textures, clipping, and
  theme colors for hover, press, focus, and disabled states.
- Built-in controls for all of the widgets listed above, including keyboard
  combo-box navigation and one-open-popup behavior.
- An Elm-style `application<State, Message>` runtime with boot, update, view,
  event-mapping, and subscription callbacks, plus focus traversal, text
  editing, combo-box selection, slider dragging, and scrolling.
- `ustring` and `ustring_view` keep UTF-8 valid and mutate by code point.
- Layout can be tested without a window because the engine accepts a
  `text_measurer` callback.

Accessibility-tree integration and bidirectional text are not implemented; see
[[Controls]] and [[Rendering-and-Themes]] for the current boundaries.

## Documentation

| Page | Contents |
| --- | --- |
| [[Getting-Started]] | Dependencies, build commands, and a complete counter app |
| [[Widgets]] | Primitive nodes, built-in factories, and modifiers |
| [[Controls]] | Control events, focus, text editing, combo boxes, and sliders |
| [[Layout]] | Sizing, text measurement, boxes, stacks, and widget identity |
| [[Rendering-and-Themes]] | Render context, focus/selection drawing, themes, and fonts |
| [[Application-Runtime]] | Application callbacks and event dispatch |
| [[Text-Utilities]] | UTF-8 strings, line breaking, and wrapping |
| [[Testing]] | Running and understanding the test suite |

The runnable widget gallery lives in `example.cc`; it puts every built-in
widget into a single window.
