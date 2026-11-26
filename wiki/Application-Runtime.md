# Application runtime

`application<State, Message>` owns the window, renderer, and event loop:

- `on_boot` returns `kei::result<std::pair<State, Message>>`.
- `on_update` receives `State &` and `const Message &`; the three-argument form
  also receives a `message_emitter` for follow-up messages.
- `on_view` returns any widget value (or `node_data`), which the runtime
  finalizes and lays out.
- `on_event` maps a framework `event` to `std::optional<Message>`.
- `on_subscription` is polled by the loop and can emit a message.

`run()` returns the process exit code. `boot`, `update`, and `view` are
required; the other callbacks are optional.

## Events

Framework events carry the hit-tested `target` widget id, the nearest `on_click`
`action`, logical pointer coordinates, scroll deltas, key codes, and text input.
A left-button release reports an `action` only when the press began on the same
action widget, so dragging off a button cancels the click. Only the primary
(left) button changes the pressed visual state. The `button` field is still
available for user mappings.

Event kinds: `quit`, `resize`, `mouse_move`, `mouse_down`, `mouse_up`, `scroll`,
`key_down`, `key_up`, `text_input`, and the control kinds described in
[[Controls]]: `text_edit`, `submit`, `selection_changed`, `slider_changed`, and
`activate`.

```cpp
.on_event([](const sleet::event &input) -> std::optional<message>
          {
              if (input.type == sleet::event::kind::mouse_up
                  and input.button == SDL_BUTTON_LEFT)
                  return action_message(input.action);

              if (input.type == sleet::event::kind::activate)
                  return action_message(input.action);

              if (input.type == sleet::event::kind::text_edit
                  and input.action == "name")
                  return message::name_changed(input.text);

              return std::nullopt;
          })
```

## Runtime details

The runtime owns the SDL window, renderer, and event loop. Layout and event
coordinates are logical window points; the renderer converts them to physical
pixels from the current output size, so high-DPI windows stay sharp without
changing application code. Resize, pixel-size, display-change, and display-scale
events update the viewport and scale, then mark the frame dirty.

Focus, text selection, IME preedit, undo/redo stacks, open combos, scroll
offsets, hover, press, and drag state are kept per widget id across view
rebuilds. The runtime validates saved ids against the new tree, so stale state
cannot survive a widget being removed or changing type. Disabled subtrees are
skipped by focus traversal, and disabled widgets never become pointer targets.

Wheel events scroll the nearest `scrollable()` ancestor and are also dispatched
as `scroll` events with logical coordinates and deltas. Scroll offsets are
clamped to the measured content after each layout and applied by translating the
subtree, so hit testing and painting use the same coordinates. Nested scroll
containers are resolved bottom-up.

Text input supports platform IME composition. `SDL_EVENT_TEXT_EDITING` updates
the preedit shown in the focused field, and `SDL_EVENT_TEXT_INPUT` commits the
final text, replacing rather than appending to the composition. Undo and redo
snapshots are recorded before text-changing operations and are exposed through
Ctrl/Cmd+Z and Ctrl/Cmd+Shift+Z or Ctrl/Cmd+Y.

`set_theme` can be called at any time from the application thread. It applies
the new theme to the live render context, rebuilds the text cache, and requests
a redraw on the next frame.

See [[Getting-Started]] for a complete application and [[Rendering-and-Themes]]
for the rendering side of the loop.
