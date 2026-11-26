# Testing

```sh
meson test -C build --print-errorlogs
```

The suite covers UTF-8 string mutation, grapheme clustering, word navigation and
deletion, color conversion, node ownership and stable id generation, layout,
floating overlays, wrapping, theme fallback, easing and animated values, text
editing, widget factories, scrollbar geometry, and an SDL software-renderer
smoke test. Rendering tests check antialiased rounded corners, eased interaction
blending, scrollbar painting, and that floating content paints above normal
flow. Application tests drive focus traversal, text input, undo/redo, IME
commit, disabled controls, combo boxes, sliders, and wheel scrolling through the
real event loop. Rendering and application tests are skipped automatically when
no system font or renderer is available.

The all-widgets example (`example.cc`) is a manual visual test: launch it and
exercise every control. For a headless smoke test:

```sh
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software ./build/sleet-example
```

When running it under `timeout`, a clean run ends with exit code 124 because the
window keeps rendering until it is closed.
