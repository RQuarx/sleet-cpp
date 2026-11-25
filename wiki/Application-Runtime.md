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

The runtime handles window resize, logical-coordinate conversion, high-DPI
presentation, focus loss, and viewport clipping. It re-applies combo visibility
and control state after each view rebuild, and it validates saved ids against
the new tree so stale focus, selection, and drag state cannot survive a widget
being removed or changing type.

See [[Getting-Started]] for a complete application and [[Rendering-and-Themes]]
for the rendering side of the loop.
