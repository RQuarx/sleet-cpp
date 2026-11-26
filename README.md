# Sleet-cpp

Sleet is a small C++23 UI framework modeled on the
[Elm architecture](https://guide.elm-lang.org/architecture/). Widgets are plain
values, layout is a pure calculation, and rendering is a thin layer over SDL3.

It provides primitive `box`, `row`, `column`, and `label` nodes, built-in
buttons, text inputs, combo boxes, checkboxes, toggles, radio buttons, sliders,
progress bars, separators, spacers, and badges, plus an Elm-style application
runtime with focus, IME-aware text editing and undo/redo, combo-box keyboard
navigation, smooth scrolling with proportional scrollbars, eased interaction
transitions, floating overlays, stable widget ids, themes, and high-DPI
antialiased rendering.

## Documentation

The full documentation is maintained as wiki pages in [`wiki/`](wiki/Home.md):

- [Home](wiki/Home.md) — overview and feature list
- [Getting started](wiki/Getting-Started.md) — dependencies, build, and a complete counter app
- [Widgets](wiki/Widgets.md) — factories, composition, and modifiers
- [Controls](wiki/Controls.md) — control events, focus, and keyboard behavior
- [Layout](wiki/Layout.md) — sizing, wrapping, and widget identity
- [Rendering and themes](wiki/Rendering-and-Themes.md) — renderer, themes, and fonts
- [Application runtime](wiki/Application-Runtime.md) — callbacks and event dispatch
- [Text utilities](wiki/Text-Utilities.md) — UTF-8 strings and line breaking
- [Testing](wiki/Testing.md) — running the suite and the widget gallery

The pages are GitHub-wiki-ready: copy them to the repository's wiki to publish.
The sidebar and footer are in `wiki/_Sidebar.md` and `wiki/_Footer.md`.

## Building

```sh
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
./build/sleet-example
```

`example.cc` is an all-widgets gallery: every built-in widget appears in one
interactive window. See [Getting started](wiki/Getting-Started.md) for build
options and headless smoke testing.

## License

GNU General Public License version 3. See [COPYING](COPYING).
