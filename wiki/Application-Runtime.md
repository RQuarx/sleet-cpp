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
`request_quit()` stops the loop after the current iteration and works from
subscriptions and update callbacks, not just window-close events.

Tooltips are runtime state. The deepest hovered node with `tooltip()` on itself
or an ancestor starts a 550 ms timer; pointer motion, clicks, scrolling, and
focus changes hide the bubble. `set_tooltip_delay(ms)` changes that delay. Only
nodes that can actually show a tooltip wake the idle loop, so resting the pointer
on ordinary widgets leaves the window asleep. Toasts created with
`notify(text, seconds)` live in the runtime too, fading and sliding in, stacking
bottom-right, and expiring without any view state.

Focus, text selection, IME preedit, undo/redo stacks, open combos, scroll
offsets, hover, press, and drag state are kept per widget id across view
rebuilds. The runtime validates saved ids against the new tree, so stale state
cannot survive a widget being removed or changing type. Disabled subtrees are
skipped by focus traversal, and disabled widgets never become pointer targets.

Overlays refine focus and input. A visible `modal()` node restricts hit testing
to its subtree: clicks outside it target the modal surface, where a release or
Escape emits its cancel action. While a modal is open, Tab and Shift+Tab cycle
inside it and the previously focused widget is restored when the modal closes.
`focus_scope()` traps Tab without blocking outside clicks. The last visible
modal in paint order wins.

Wheel events scroll the nearest `scrollable()` ancestor by 48 logical pixels per
step and are also dispatched as `scroll` events with logical coordinates and
deltas. The runtime moves 40% of the step immediately and eases the remainder,
so input feels responsive without jumping. Offsets are clamped to the measured
content after each layout and applied by translating the subtree, so hit testing
and painting use the same coordinates. Nested scroll containers are resolved
bottom-up. When focus moves, scrollable ancestors reveal the focused widget
unless it opts out with `ensure_visible(false)`. `set_reduced_motion()` snaps
scrolling and control transitions to their targets.

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
