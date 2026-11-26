#include <cmath>
#include <optional>
#include <string>
#include <utility>

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <boost/ut.hpp>

#include "sleet/sleet.hh"


namespace
{
    auto push_mouse(bool down, float x, float y) -> void
    {
        SDL_Event native {};
        native.type          = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        native.button.button = SDL_BUTTON_LEFT;
        native.button.down   = down;
        native.button.x      = x;
        native.button.y      = y;
        SDL_PushEvent(&native);
    }


    auto push_key(SDL_Keycode key, SDL_Keymod mod = SDL_KMOD_NONE) -> void
    {
        SDL_Event native {};
        native.type    = SDL_EVENT_KEY_DOWN;
        native.key.key = key;
        native.key.mod = mod;
        SDL_PushEvent(&native);
    }


    auto push_text(const char *text) -> void
    {
        SDL_Event native {};
        native.type      = SDL_EVENT_TEXT_INPUT;
        native.text.text = text;
        SDL_PushEvent(&native);
    }


    auto push_editing(const char *text, int start = 0, int length = 0) -> void
    {
        SDL_Event native {};
        native.type        = SDL_EVENT_TEXT_EDITING;
        native.edit.text   = text;
        native.edit.start  = start;
        native.edit.length = length;
        SDL_PushEvent(&native);
    }


    auto push_wheel(float mouse_x, float mouse_y, float x, float y) -> void
    {
        SDL_Event native {};
        native.type          = SDL_EVENT_MOUSE_WHEEL;
        native.wheel.mouse_x = mouse_x;
        native.wheel.mouse_y = mouse_y;
        native.wheel.x       = x;
        native.wheel.y       = y;
        SDL_PushEvent(&native);
    }


    auto push_quit() -> void
    {
        SDL_Event native {};
        native.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&native);
    }
}


const boost::ut::suite application_tests = []
{
    using namespace boost::ut;

    "the runtime drives focus, editing, and keyboard activation"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            std::string name;
            int         submits   = 0;
            int         activates = 0;
        };

        struct message
        {
            std::string action;
            std::string text;
        };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto result = app { "sleet-application-test", 320, 240, std::move(theme) }
                          .on_boot([]() -> kei::result<std::pair<state, message>>
                                   { return std::pair { state {}, message {} }; })
                          .on_update(
                              [](state &current, const message &value) -> kei::result<>
                              {
                                  if (value.action.empty())
                                      current.name = value.text;
                                  else if (value.action == "submit")
                                      current.submits++;
                                  else if (value.action == "activate")
                                      current.activates++;
                                  return {};
                              })
                          .on_view(
                              [&observed](const state &current)
                              {
                                  observed = current;
                                  return column()[text_input(current.name, "name", "Name"),
                                                  button("Activate", "activate")];
                              })
                          .on_event(
                              [](const sleet::event &input) -> std::optional<message>
                              {
                                  switch (input.type)
                                  {
                                  case sleet::event::kind::text_edit:
                                      if (input.action == "name") return message { {}, input.text };
                                      return std::nullopt;
                                  case sleet::event::kind::submit:
                                      if (input.action == "name") return message { "submit", {} };
                                      return std::nullopt;
                                  case sleet::event::kind::activate:
                                      if (input.action == "activate")
                                          return message { "activate", {} };
                                      return std::nullopt;
                                  default: return std::nullopt;
                                  }
                              })
                          .on_subscription(
                              [&frame]() -> std::optional<message>
                              {
                                  switch (++frame)
                                  {
                                  case 1:  push_key(SDLK_TAB); break;
                                  case 2:  push_text("hi"); break;
                                  case 3:  push_key(SDLK_RETURN); break;
                                  case 4:  push_key(SDLK_TAB); break;
                                  case 5:  push_key(SDLK_RETURN); break;
                                  case 6:  push_quit(); break;
                                  default: break;
                                  }

                                  return std::nullopt;
                              })
                          .run();

        expect(eq(result, 0));
        expect(eq(observed.name, std::string { "hi" }));
        expect(eq(observed.submits, 1));
        expect(eq(observed.activates, 1));
    };

    "clicking a checkbox indicator toggles its action"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        { bool checked = false; };

        struct message
        { std::string action; };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto result
            = app { "sleet-checkbox-test", 320, 240, std::move(theme) }
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message {} }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.action == "check") current.checked = !current.checked;
                          return {};
                      })
                  .on_view(
                      [&observed](const state &current)
                      {
                          observed = current;
                          return column()[checkbox("Check", current.checked, "check") | width(200.F)
                                          | height(40.F)];
                      })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::mouse_up
                              and input.button == SDL_BUTTON_LEFT and input.action == "check")
                              return message { "check" };
                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1:
                              push_mouse(true, 10.F, 20.F);
                              push_mouse(false, 10.F, 20.F);
                              break;
                          case 2:  push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(observed.checked);
    };

    "combo boxes and sliders handle pointer input"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            std::string color;
            float       volume = 0.F;
        };

        struct message
        {
            std::string action;
            std::string text;
            float       value = 0.F;
        };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto result = app { "sleet-controls-test", 320, 240, std::move(theme) }
                          .on_boot([]() -> kei::result<std::pair<state, message>>
                                   { return std::pair { state {}, message {} }; })
                          .on_update(
                              [](state &current, const message &value) -> kei::result<>
                              {
                                  if (value.action == "color")
                                      current.color = value.text;
                                  else if (value.action == "volume")
                                      current.volume = value.value;
                                  return {};
                              })
                          .on_view(
                              [&observed](const state &current)
                              {
                                  observed = current;
                                  return column()[slider(current.volume, "volume") | height(20.F),
                                                  combo_box(current.color, { "Red", "Green" },
                                                            "color", "Pick a color")]
                                       | width(200.F);
                              })
                          .on_event(
                              [](const sleet::event &input) -> std::optional<message>
                              {
                                  if (input.type == sleet::event::kind::slider_changed
                                      and input.action == "volume")
                                      return message { "volume", {}, input.value };
                                  if (input.type == sleet::event::kind::selection_changed
                                      and input.action == "color")
                                      return message { "color", input.text };
                                  return std::nullopt;
                              })
                          .on_subscription(
                              [&frame]() -> std::optional<message>
                              {
                                  switch (++frame)
                                  {
                                  case 1:
                                      push_mouse(true, 100.F, 10.F);
                                      push_mouse(false, 100.F, 10.F);
                                      break;
                                  case 2:
                                      push_mouse(true, 50.F, 30.F);
                                      push_mouse(false, 50.F, 30.F);
                                      break;
                                  case 3:
                                      push_mouse(true, 50.F, 70.F);
                                      push_mouse(false, 50.F, 70.F);
                                      break;
                                  case 4:  push_quit(); break;
                                  default: break;
                                  }

                                  return std::nullopt;
                              })
                          .run();

        expect(eq(result, 0));
        expect(eq(observed.color, std::string { "Red" }));
        expect(std::abs(observed.volume - 0.5F) < 0.01F);
    };

    "undo and redo restore text snapshots"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        { std::string name; };

        struct message
        {
            std::string action;
            std::string text;
        };

        using app = sleet::application<state, message>;

        auto observed   = state {};
        auto after_undo = std::string {};
        auto frame      = 0;

        auto result = app { "sleet-undo-test", 320, 240, std::move(theme) }
                          .on_boot([]() -> kei::result<std::pair<state, message>>
                                   { return std::pair { state {}, message {} }; })
                          .on_update(
                              [](state &current, const message &value) -> kei::result<>
                              {
                                  if (value.action.empty()) current.name = value.text;
                                  return {};
                              })
                          .on_view(
                              [&observed](const state &current)
                              {
                                  observed = current;
                                  return column()[text_input(current.name, "name", "Name")];
                              })
                          .on_event(
                              [](const sleet::event &input) -> std::optional<message>
                              {
                                  if (input.type == sleet::event::kind::text_edit
                                      and input.action == "name")
                                      return message { {}, input.text };
                                  return std::nullopt;
                              })
                          .on_subscription(
                              [&frame, &observed, &after_undo]() -> std::optional<message>
                              {
                                  switch (++frame)
                                  {
                                  case 1: push_key(SDLK_TAB); break;
                                  case 2: push_text("abc"); break;
                                  case 3: push_key(SDLK_Z, SDL_KMOD_CTRL); break;
                                  case 4: push_key(SDLK_Y, SDL_KMOD_CTRL); break;
                                  case 5:
                                      after_undo = observed.name;
                                      push_quit();
                                      break;
                                  default: break;
                                  }

                                  return std::nullopt;
                              })
                          .run();

        expect(eq(result, 0));
        expect(eq(after_undo, std::string {}));
        expect(eq(observed.name, std::string { "abc" }));
    };

    "an empty clipboard paste does not create an undo step"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        { std::string name; };

        struct message
        {
            std::string action;
            std::string text;
        };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto result = app { "sleet-paste-test", 320, 240, std::move(theme) }
                          .on_boot([]() -> kei::result<std::pair<state, message>>
                                   { return std::pair { state {}, message {} }; })
                          .on_update(
                              [](state &current, const message &value) -> kei::result<>
                              {
                                  if (value.action.empty()) current.name = value.text;
                                  return {};
                              })
                          .on_view(
                              [&observed](const state &current)
                              {
                                  observed = current;
                                  return column()[text_input(current.name, "name", "Name")];
                              })
                          .on_event(
                              [](const sleet::event &input) -> std::optional<message>
                              {
                                  if (input.type == sleet::event::kind::text_edit
                                      and input.action == "name")
                                      return message { {}, input.text };
                                  return std::nullopt;
                              })
                          .on_subscription(
                              [&frame]() -> std::optional<message>
                              {
                                  switch (++frame)
                                  {
                                  case 1:
                                      // The headless clipboard may be
                                      // unavailable; an empty clipboard is the
                                      // only thing this test needs.
                                      (void)SDL_SetClipboardText("");
                                      push_key(SDLK_TAB);
                                      break;
                                  case 2:  push_text("abc"); break;
                                  case 3:  push_key(SDLK_V, SDL_KMOD_CTRL); break;
                                  case 4:  push_key(SDLK_Z, SDL_KMOD_CTRL); break;
                                  case 5:  push_quit(); break;
                                  default: break;
                                  }

                                  return std::nullopt;
                              })
                          .run();

        expect(eq(result, 0));

        // If the no-op paste had recorded an undo snapshot, Ctrl+Z would have
        // restored "abc" instead of the text from before typing.
        expect(eq(observed.name, std::string {}));
    };

    "disabled controls ignore pointer and keyboard input"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            std::string name        = "kept";
            int         activations = 0;
        };

        struct message
        {
            std::string action;
            std::string text;
        };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto result
            = app { "sleet-disabled-test", 320, 240, std::move(theme) }
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message {} }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.action == "name") current.name = value.text;
                          if (value.action == "activate") current.activations++;
                          return {};
                      })
                  .on_view(
                      [&observed](const state &current)
                      {
                          observed = current;
                          return column()[text_input(current.name, "name", "Name") | disabled(),
                                          button("Activate", "activate") | disabled()]
                               | width(200.F);
                      })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::text_edit
                              and input.action == "name")
                              return message { "name", input.text };
                          if (input.type == sleet::event::kind::activate
                              and input.action == "activate")
                              return message { "activate", {} };
                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1: push_key(SDLK_TAB); break;
                          case 2: push_text("x"); break;
                          case 3:
                              push_mouse(true, 20.F, 20.F);
                              push_mouse(false, 20.F, 20.F);
                              break;
                          case 4: push_text("y"); break;
                          case 5:
                              push_mouse(true, 20.F, 50.F);
                              push_mouse(false, 20.F, 50.F);
                              break;
                          case 6:  push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(eq(observed.name, std::string { "kept" }));
        expect(eq(observed.activations, 0));
    };

    "IME composition commits without duplicating preedit text"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        { std::string name; };

        struct message
        {
            std::string action;
            std::string text;
        };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto result = app { "sleet-ime-test", 320, 240, std::move(theme) }
                          .on_boot([]() -> kei::result<std::pair<state, message>>
                                   { return std::pair { state {}, message {} }; })
                          .on_update(
                              [](state &current, const message &value) -> kei::result<>
                              {
                                  if (value.action.empty()) current.name = value.text;
                                  return {};
                              })
                          .on_view(
                              [&observed](const state &current)
                              {
                                  observed = current;
                                  return column()[text_input(current.name, "name", "Name")];
                              })
                          .on_event(
                              [](const sleet::event &input) -> std::optional<message>
                              {
                                  if (input.type == sleet::event::kind::text_edit
                                      and input.action == "name")
                                      return message { {}, input.text };
                                  return std::nullopt;
                              })
                          .on_subscription(
                              [&frame]() -> std::optional<message>
                              {
                                  switch (++frame)
                                  {
                                  case 1:  push_key(SDLK_TAB); break;
                                  case 2:  push_editing("\xE3\x81\xAB\xE3\x81\xBB", 0, 2); break;
                                  case 3:  push_text("\xE6\x97\xA5\xE6\x9C\xAC"); break;
                                  case 4:  push_quit(); break;
                                  default: break;
                                  }

                                  return std::nullopt;
                              })
                          .run();

        expect(eq(result, 0));
        expect(eq(observed.name, std::string { "\xE6\x97\xA5\xE6\x9C\xAC" }));
    };

    "wheel scrolling moves content for hit testing"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        { int hits = 0; };

        struct message
        { std::string action; };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto instance = app { "sleet-scroll-test", 320, 240, std::move(theme) };
        instance
            .on_boot([]() -> kei::result<std::pair<state, message>>
                     { return std::pair { state {}, message {} }; })
            .on_update(
                [](state &current, const message &value) -> kei::result<>
                {
                    if (value.action == "hit") current.hits++;
                    return {};
                })
            .on_view(
                [&observed](const state &current)
                {
                    observed = current;
                    return column()[column()[box() | width(200.F) | height(100.F),
                                             button("Hit", "hit") | width(200.F) | height(20.F)]
                                    | width(200.F) | height(60.F) | scrollable()]
                         | width(200.F) | height(200.F);
                })
            .on_event(
                [](const sleet::event &input) -> std::optional<message>
                {
                    if (input.type == sleet::event::kind::mouse_up and input.action == "hit")
                        return message { "hit" };
                    return std::nullopt;
                })
            .on_subscription(
                [&frame]() -> std::optional<message>
                {
                    switch (++frame)
                    {
                    case 1: push_wheel(50.F, 20.F, 0.F, -1.F); break;
                    case 2:
                        push_mouse(true, 50.F, 55.F);
                        push_mouse(false, 50.F, 55.F);
                        break;
                    case 3:  push_quit(); break;
                    default: break;
                    }

                    return std::nullopt;
                });

        // Snap scrolling so the click lands on a deterministic frame.
        instance.set_reduced_motion();

        const auto result = instance.run();

        expect(eq(result, 0));
        expect(eq(observed.hits, 1));
    };

    "focus reveals an off-screen row inside a scroll viewport"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        { std::string hit; };

        struct message
        { std::string action; };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto instance = app { "sleet-reveal-test", 200, 60, std::move(theme) };
        instance
            .on_boot([]() -> kei::result<std::pair<state, message>>
                     { return std::pair { state {}, message {} }; })
            .on_update(
                [](state &current, const message &value) -> kei::result<>
                {
                    current.hit = value.action;
                    return {};
                })
            .on_view(
                [&observed](const state &current)
                {
                    observed = current;
                    return column()[button("One", "one") | width(200.F) | height(36.F),
                                    button("Two", "two") | width(200.F) | height(36.F),
                                    button("Three", "three") | width(200.F) | height(36.F)]
                         | id("viewport") | width(200.F) | height(60.F) | clip() | scrollable();
                })
            .on_event(
                [](const sleet::event &input) -> std::optional<message>
                {
                    if (input.type == sleet::event::kind::mouse_up and !input.action.empty())
                        return message { input.action };
                    return std::nullopt;
                })
            .on_subscription(
                [&frame]() -> std::optional<message>
                {
                    switch (++frame)
                    {
                    case 1: push_key(SDLK_TAB); break;
                    case 2: push_key(SDLK_TAB); break;
                    case 3: push_key(SDLK_TAB); break;
                    case 4:
                        push_mouse(true, 100.F, 40.F);
                        push_mouse(false, 100.F, 40.F);
                        break;
                    case 5:  push_quit(); break;
                    default: break;
                    }

                    return std::nullopt;
                });

        instance.set_reduced_motion();

        const auto result = instance.run();

        expect(eq(result, 0));
        expect(eq(observed.hit, std::string { "three" }));
    };

    "combo box Home and End focus the first and last options"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        { std::string color; };

        struct message
        {
            std::string action;
            std::string text;
        };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto result
            = app { "sleet-combo-key-test", 320, 240, std::move(theme) }
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message {} }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.action == "color") current.color = value.text;
                          return {};
                      })
                  .on_view(
                      [&observed](const state &current)
                      {
                          observed = current;
                          return column()[combo_box(current.color, { "Red", "Green", "Blue" },
                                                    "color", "Pick a color")]
                               | width(200.F);
                      })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::selection_changed
                              and input.action == "color")
                              return message { "color", input.text };
                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1:  push_key(SDLK_TAB); break;
                          case 2:  push_key(SDLK_SPACE); break;
                          case 3:  push_key(SDLK_END); break;
                          case 4:  push_key(SDLK_HOME); break;
                          case 5:  push_key(SDLK_RETURN); break;
                          case 6:  push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(eq(observed.color, std::string { "Red" }));
    };
};


auto main() -> int {}
