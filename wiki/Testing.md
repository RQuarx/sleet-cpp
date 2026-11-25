# Testing

```sh
meson test -C build --print-errorlogs
```

The suite covers UTF-8 string mutation, color conversion, node ownership and id
finalization, layout, wrapping, theme fallback, UTF-8 text editing, widget
factories, and an SDL software-renderer smoke test. An application test drives
focus traversal, text input, and keyboard activation through the real event
loop. Rendering and application tests are skipped automatically when no system
font or renderer is available.

The all-widgets example (`example.cc`) is a manual visual test: launch it and
exercise every control. For a headless smoke test:

```sh
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software ./build/sleet-example
```

When running it under `timeout`, a clean run ends with exit code 124 because the
window keeps rendering until it is closed.
