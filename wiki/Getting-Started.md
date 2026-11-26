# Getting started

## Dependencies

| Dependency | Purpose |
| --- | --- |
| C++23 compiler | `std::expected`, `std::format`, `std::println` |
| [Meson](https://mesonbuild.com/) >= 1.4 | Build system |
| [SDL3](https://wiki.libsdl.org/SDL3/) | Window, renderer, events |
| [SDL3_ttf](https://wiki.libsdl.org/SDL3_ttf/) | Font loading and text rasterization |
| [libunibreak](https://github.com/adah1972/libunibreak) | Unicode line-breaking |
| [utf8cpp](https://github.com/nemtrif/utf8cpp) | UTF-8 iteration and validation |
| [kei-utils](https://github.com/RQuarx/kei-utils) | `result`, errors, runtime guards |
| [boost.ut](https://github.com/boost-ext/ut) | Tests only |

Meson wraps for the non-system dependencies live in `subprojects/`. When a
dependency is not installed, Meson can fetch it through the matching wrap.

## Building

```sh
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
./build/sleet-example
```

To set up a build without tests or the example:

```sh
meson setup build -Dbuild-tests=false -Dbuild-example=false
```

The example is an all-widgets gallery: every built-in widget is present in one
window, and the controls are interactive. On a headless machine, smoke-test it
with SDL's dummy drivers:

```sh
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software ./build/sleet-example
```

## High-DPI and text input

Widget sizes, margins, and pointer coordinates are logical window points. The
runtime reads the renderer's physical output size and rasterizes fonts and
shapes at that scale, so a high-DPI window looks sharp without any application
changes. The renderer does not enable SDL's logical presentation.

Text inputs work with the platform IME: composition text is shown as preedit
and replaced on commit. Editing is grapheme-aware, and the usual word movement,
selection, clipboard, undo, and redo shortcuts are built in. See [[Controls]]
for the full keyboard contract.

## A complete counter app

```cpp
#include <format>
#include <optional>
#include <string>
#include <utility>

#include <sleet/sleet.hh>

struct state
{
    int count = 0;
};

enum class message
{
    none,
    increment,
    decrement,
    reset,
};

using app = sleet::application<state, message>;

auto view(const state &current) -> sleet::widget::column_node
{
    using namespace sleet::literals;
    using namespace sleet::widget;

    return column()[label(std::format("Count: {}", current.count)) | text_size(32.F)
                        | text_role(font_role::heading) | text_color(0xE6E8EB_rgb)
                        | halign(align::center),
                    row()[button("-", "decrement"), button("Reset", "reset"),
                          button("+", "increment", button_variant::primary)]
                        | justify_content(justify::center) | gap(10.F)]
         | padding(24.F) | gap(20.F) | size(sizing::fill()) | halign(align::center)
         | background(0x17191C_rgb);
}

int main()
{
    return app { "Sleet counter", 640, 480 }
        .on_boot([]() -> kei::result<std::pair<state, message>>
                 { return std::pair { state {}, message::none }; })
        .on_update([](state &current, const message &value) -> kei::result<>
                   {
                       switch (value)
                       {
                       case message::none:      break;
                       case message::increment: current.count++; break;
                       case message::decrement: current.count--; break;
                       case message::reset:     current.count = 0; break;
                       }
                       return {};
                   })
        .on_view(view)
        .on_event([](const sleet::event &input) -> std::optional<message>
                  {
                      if (input.type != sleet::event::kind::mouse_up
                          and input.type != sleet::event::kind::activate)
                          return std::nullopt;

                      if (input.action == "increment") return message::increment;
                      if (input.action == "decrement") return message::decrement;
                      if (input.action == "reset") return message::reset;
                      return std::nullopt;
                  })
        .run();
}
```

See [[Widgets]] for the full widget set and [[Application-Runtime]] for the
callback contract.
