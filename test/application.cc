#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
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


    auto push_motion(float x, float y) -> void
    {
        SDL_Event native {};
        native.type     = SDL_EVENT_MOUSE_MOTION;
        native.motion.x = x;
        native.motion.y = y;
        SDL_PushEvent(&native);
    }


    auto push_key(SDL_Keycode key, SDL_Keymod mod = SDL_KMOD_NONE, bool repeat = false) -> void
    {
        SDL_Event native {};
        native.type       = SDL_EVENT_KEY_DOWN;
        native.key.key    = key;
        native.key.mod    = mod;
        native.key.repeat = repeat;
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


    auto window_by_title(std::string_view title) -> SDL_Window *
    {
        auto  count   = 0;
        auto *windows = SDL_GetWindows(&count);
        if (windows == nullptr) return nullptr;

        SDL_Window *found = nullptr;
        for (auto i = 0; i < count; i++)
        {
            const auto *window_title = SDL_GetWindowTitle(windows[i]);
            if (window_title != nullptr and title == window_title)
            {
                found = windows[i];
                break;
            }
        }

        SDL_free(windows);
        return found;
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

    "Enter inserts a newline in a text area and Ctrl+Enter submits"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            std::string text;
            int         submits = 0;
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
            = app { "sleet-text-area-test", 320, 240, std::move(theme) }
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message {} }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.action.empty())
                              current.text = value.text;
                          else if (value.action == "notes")
                              current.submits++;
                          return {};
                      })
                  .on_view(
                      [&observed](const state &current)
                      {
                          observed = current;
                          return column()[text_area(current.text, "notes", "Notes")];
                      })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::text_edit and input.action == "notes")
                              return message { {}, input.text };
                          if (input.type == sleet::event::kind::submit and input.action == "notes")
                              return message { "notes", {} };
                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1:  push_key(SDLK_TAB); break;
                          case 2:  push_text("a"); break;
                          case 3:  push_key(SDLK_RETURN); break;
                          case 4:  push_text("b"); break;
                          case 5:  push_key(SDLK_RETURN, SDL_KMOD_CTRL); break;
                          case 6:  push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(eq(observed.text, std::string { "a\nb" }));
        expect(eq(observed.submits, 1));
    };

    "a text area moves by visual lines and keeps Home and End line-aware"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            std::string text = "ab\ncd";
            int         submits = 0;
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
            = app { "sleet-text-area-lines-test", 320, 240, std::move(theme) }
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message { "boot", {} } }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.action.empty())
                              current.text = value.text;
                          else if (value.action == "notes")
                              current.submits++;
                          return {};
                      })
                  .on_view(
                      [&observed](const state &current)
                      {
                          observed = current;
                          return column()[text_area(current.text, "notes", "Notes")];
                      })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::text_edit and input.action == "notes")
                              return message { {}, input.text };
                          if (input.type == sleet::event::kind::submit and input.action == "notes")
                              return message { "notes", {} };
                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1:  push_key(SDLK_TAB); break;
                          // Focus places the caret at the end, so move to the
                          // document start before exercising line navigation.
                          case 2:  push_key(SDLK_HOME, SDL_KMOD_CTRL); break;
                          case 3:  push_key(SDLK_END); break;
                          case 4:  push_text("X"); break;
                          case 5:  push_key(SDLK_DOWN); break;
                          case 6:  push_key(SDLK_HOME); break;
                          case 7:  push_text("Y"); break;
                          case 8:  push_key(SDLK_UP); break;
                          case 9:  push_text("Z"); break;
                          case 10: push_key(SDLK_RETURN, SDL_KMOD_CTRL); break;
                          case 11: push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(eq(observed.submits, 1));

        const auto newline = observed.text.find('\n');
        expect(newline != std::string::npos);
        if (newline == std::string::npos) return;

        expect(observed.text.find('Z') < newline);
        expect(eq(std::string_view { observed.text }.substr(newline + 1),
                  std::string_view { "Ycd" }));
    };

    "Home stays on the empty line after a trailing newline"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            std::string text = "ab\n";
        };

        struct message
        {
            std::string text;
        };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto result
            = app { "sleet-text-area-trailing-test", 320, 240, std::move(theme) }
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message { "boot" } }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.text != "boot") current.text = value.text;
                          return {};
                      })
                  .on_view(
                      [&observed](const state &current)
                      {
                          observed = current;
                          return column()[text_area(current.text, "notes")];
                      })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::text_edit
                              and input.action == "notes")
                              return message { input.text };
                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1:  push_key(SDLK_TAB); break;
                          case 2:  push_key(SDLK_HOME); break;
                          case 3:  push_text("X"); break;
                          case 4:  push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(eq(observed.text, std::string { "ab\nX" }));
    };

    "a hovered widget without a tooltip does not wake the idle loop"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
        };

        struct message
        {
        };

        using app = sleet::application<state, message>;

        auto polls       = 0;
        auto motion_sent = false;
        auto motion_tick = Uint64 { 0 };

        auto instance = app { "sleet-idle-tooltip-test", 320, 240, std::move(theme) };

        // A zero delay makes the regression deterministic: once the pointer
        // rests on a widget without tooltip text, the old timeout calculation
        // returned zero forever and stopped polling SDL events entirely.
        instance.set_tooltip_delay(0);

        auto result = instance
                          .on_boot([]() -> kei::result<std::pair<state, message>>
                                   { return std::pair { state {}, message {} }; })
                          .on_update([](state &, const message &) -> kei::result<> { return {}; })
                          .on_view([](const state &)
                                   { return column()[box() | width(200.F) | height(40.F)]; })
                          .on_subscription(
                              [&]() -> std::optional<message>
                              {
                                  polls++;

                                  if (!motion_sent)
                                  {
                                      push_motion(10.F, 20.F);
                                      motion_sent = true;
                                      motion_tick = SDL_GetTicks();
                                  }
                                  else if (SDL_GetTicks() - motion_tick >= 400)
                                      instance.request_quit();

                                  return std::nullopt;
                              })
                          .run();

        expect(eq(result, 0));

        // At the subscription cadence this is about 25 polls. A busy loop
        // reaches thousands in the same window.
        expect(polls < 100) << "idle loop polled " << polls << " times";
    };

    "text input is enabled only while a field has focus"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        { std::string name; };

        struct message
        {
        };

        using app = sleet::application<state, message>;

        auto active_when_focused   = false;
        auto inactive_when_blurred = false;
        auto frame                 = 0;

        auto result
            = app { "sleet-text-input-state-test", 320, 240, std::move(theme) }
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message {} }; })
                  .on_update([](state &, const message &) -> kei::result<> { return {}; })
                  .on_view(
                      [](const state &current)
                      {
                          return column()[text_input(current.name, "name", "Name"),
                                          button("Other", "other")];
                      })
                  .on_subscription(
                      [&]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1: push_key(SDLK_TAB); break;
                          // Focus changes are presented on the next
                          // frame, so the state is observable here.
                          case 3:
                              if (auto *window = window_by_title("sleet-text-input-state-test");
                                  window != nullptr)
                                  active_when_focused = SDL_TextInputActive(window);
                              push_key(SDLK_TAB);
                              break;
                          case 5:
                              if (auto *window = window_by_title("sleet-text-input-state-test");
                                  window != nullptr)
                                  inactive_when_blurred = !SDL_TextInputActive(window);
                              break;
                          case 6:  push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(active_when_focused) << "text input was not enabled for the focused field";
        expect(inactive_when_blurred) << "text input stayed enabled without a focused field";
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

        auto observed        = state {};
        auto frame           = 0;
        auto clipboard_ready = true;

        auto instance = app { "sleet-paste-test", 320, 240, std::move(theme) };

        auto result = instance
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
                              [&]() -> std::optional<message>
                              {
                                  switch (++frame)
                                  {
                                  case 1: push_key(SDLK_TAB); break;

                                  // Clear the clipboard only after the window has
                                  // focus. Clipboard managers may ignore empty
                                  // selections, so verify the clear took before
                                  // relying on it.
                                  case 2: (void)SDL_SetClipboardText(""); break;
                                  case 3:
                                      if (auto *text = SDL_GetClipboardText(); text != nullptr)
                                      {
                                          clipboard_ready = *text == '\0';
                                          SDL_free(text);
                                      }
                                      else
                                          clipboard_ready = false;

                                      if (clipboard_ready)
                                          push_text("abc");
                                      else
                                          instance.request_quit();
                                      break;
                                  case 4:
                                      if (clipboard_ready) push_key(SDLK_V, SDL_KMOD_CTRL);
                                      break;
                                  case 5:
                                      if (clipboard_ready) push_key(SDLK_Z, SDL_KMOD_CTRL);
                                      break;
                                  case 6:  instance.request_quit(); break;
                                  default: break;
                                  }

                                  return std::nullopt;
                              })
                          .run();

        expect(eq(result, 0));
        if (!clipboard_ready) return;

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


const boost::ut::suite overlay_tests = []
{
    using namespace boost::ut;

    "modals trap focus, cancel on Escape, and restore focus"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            bool open    = false;
            int  ones    = 0;
            int  twos    = 0;
            int  cancels = 0;
        };

        struct message
        { std::string action; };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto result
            = app { "sleet-modal-test", 320, 240, std::move(theme) }
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message {} }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.action == "open")
                              current.open = true;
                          else if (value.action == "one")
                              current.ones++;
                          else if (value.action == "two")
                              current.twos++;
                          else if (value.action == "cancel")
                          {
                              current.open = false;
                              current.cancels++;
                          }
                          return {};
                      })
                  .on_view(
                      [&observed](const state &current)
                      {
                          observed = current;

                          auto view = column();
                          view      = view[button("Open", "open") | width(120.F) | height(30.F)];

                          if (current.open)
                          {
                              auto dialog
                                  = column()[button("One", "one") | width(90.F) | height(30.F),
                                             button("Two", "two") | width(90.F) | height(30.F)]
                                  | modal("cancel") | background_of(tone::field) | padding(12.F)
                                  | width(200.F);
                              view = view[std::move(dialog)];
                          }

                          return view;
                      })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::activate
                              and (input.action == "open" or input.action == "one"
                                   or input.action == "two"))
                              return message { input.action };

                          if (input.type == sleet::event::kind::key_down
                              and input.action == "cancel")
                              return message { "cancel" };

                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1:  push_key(SDLK_TAB); break;
                          case 2:  push_key(SDLK_RETURN); break;
                          case 3:  push_key(SDLK_RETURN); break;
                          case 4:  push_key(SDLK_TAB); break;
                          case 5:  push_key(SDLK_RETURN); break;
                          case 6:  push_key(SDLK_TAB); break;
                          case 7:  push_key(SDLK_RETURN); break;
                          case 8:  push_key(SDLK_ESCAPE); break;
                          case 9:  push_key(SDLK_RETURN); break;
                          case 10: push_key(SDLK_ESCAPE); break;
                          case 11: push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(eq(observed.ones, 2));
        expect(eq(observed.twos, 1));
        expect(eq(observed.cancels, 2));
        expect(!observed.open);
    };

    "global shortcuts pass through text input and respect exact modifiers"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            std::string text;
            int         saves            = 0;
            int         plains           = 0;
            int         tabbed           = 0;
            int         submit_shortcuts = 0;
            int         function_keys    = 0;
            int         submits          = 0;
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
            = app { "sleet-global-shortcut-test", 320, 240, std::move(theme) }
                  .on_shortcut("Ctrl+S", "save")
                  .on_shortcut("x", "plain")
                  .on_shortcut("Ctrl+Tab", "tabbed")
                  .on_shortcut("Ctrl+Enter", "submit_shortcut")
                  .on_shortcut("F5", "function_key")
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message {} }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.action == "text")
                              current.text = value.text;
                          else if (value.action == "save")
                              current.saves++;
                          else if (value.action == "plain")
                              current.plains++;
                          else if (value.action == "tabbed")
                              current.tabbed++;
                          else if (value.action == "submit_shortcut")
                              current.submit_shortcuts++;
                          else if (value.action == "function_key")
                              current.function_keys++;
                          else if (value.action == "submit")
                              current.submits++;
                          return {};
                      })
                  .on_view(
                      [&observed](const state &current)
                      {
                          observed = current;
                          return column()[text_input(current.text, "name")];
                      })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::text_edit
                              and input.action == "name")
                              return message { "text", input.text };

                          if (input.type == sleet::event::kind::submit
                              and input.action == "name")
                              return message { "submit", {} };

                          if (input.type == sleet::event::kind::key_down
                              and !input.action.empty())
                              return message { input.action, {} };

                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1:  push_key(SDLK_TAB); break;
                          case 2:  push_text("ab"); break;
                          case 3:  push_key(SDLK_S, SDL_KMOD_CTRL); break;
                          case 4:  push_key(SDLK_S, SDL_KMOD_CTRL | SDL_KMOD_SHIFT); break;
                          case 5:  push_key(SDLK_RETURN, SDL_KMOD_CTRL); break;
                          case 6:  push_key(SDLK_X); break;
                          case 7:  push_text("c"); break;
                          case 8:  push_key(SDLK_TAB, SDL_KMOD_CTRL); break;
                          case 9:  push_key(SDLK_F5); break;
                          case 10: push_key(SDLK_ESCAPE); break;
                          case 11: push_key(SDLK_X); break;
                          case 12: push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(eq(observed.text, std::string { "abc" }));
        expect(eq(observed.saves, 1));
        expect(eq(observed.plains, 1));
        expect(eq(observed.tabbed, 1));
        expect(eq(observed.submit_shortcuts, 1));
        expect(eq(observed.function_keys, 1));
        expect(eq(observed.submits, 0));
    };

    "view shortcuts follow visibility and override global bindings"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            bool visible = true;
            int  local   = 0;
            int  global  = 0;
        };

        struct message
        { std::string action; };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto result
            = app { "sleet-view-shortcut-test", 320, 240, std::move(theme) }
                  .on_shortcut("F2", "global")
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message {} }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.action == "hide")
                              current.visible = false;
                          else if (value.action == "local")
                              current.local++;
                          else if (value.action == "global")
                              current.global++;
                          return {};
                      })
                  .on_view(
                      [&observed](const state &current)
                      {
                          observed = current;
                          return column()[box()[label("panel")] | visible(current.visible)
                                          | on_shortcut(SDLK_F2, SDL_KMOD_NONE, "local")];
                      })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::key_down
                              and !input.action.empty())
                              return message { input.action };
                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1:  push_key(SDLK_F2); break;
                          case 2:  return message { "hide" };
                          case 3:  push_key(SDLK_F2); break;
                          case 4:  push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(eq(observed.local, 1));
        expect(eq(observed.global, 1));
    };

    "autofocus focuses the first visible candidate without Tab"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            std::string first;
            std::string second;
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
            = app { "sleet-autofocus-test", 320, 240, std::move(theme) }
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message {} }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.action == "first")
                              current.first = value.text;
                          else if (value.action == "second")
                              current.second = value.text;
                          return {};
                      })
                  .on_view(
                      [&observed](const state &current)
                      {
                          observed = current;
                          return column()[text_input(current.first, "first") | autofocus()
                                              | visible(false),
                                          text_input(current.second, "second") | autofocus()];
                      })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::text_edit)
                              return message { input.action, input.text };
                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1:  push_text("x"); break;
                          case 2:  push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(observed.first.empty());
        expect(eq(observed.second, std::string { "x" }));
    };

    "a modal autofocus target beats the first focusable widget"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            bool        open = true;
            std::string background;
            std::string modal;
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
            = app { "sleet-modal-autofocus-test", 320, 240, std::move(theme) }
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message {} }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.action == "background")
                              current.background = value.text;
                          else if (value.action == "modal")
                              current.modal = value.text;
                          else if (value.action == "cancel")
                              current.open = false;
                          return {};
                      })
                  .on_view(
                      [&observed](const state &current)
                      {
                          observed = current;

                          auto view = column()[text_input(current.background, "background")];
                          if (current.open)
                          {
                              auto dialog
                                  = column()[button("Cancel", "cancel"),
                                             text_input(current.modal, "modal") | autofocus()]
                                  | modal("cancel") | width(220.F);

                              view = view[std::move(dialog)];
                          }

                          return view;
                      })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::text_edit)
                              return message { input.action, input.text };

                          if (input.type == sleet::event::kind::key_down
                              and input.action == "cancel")
                              return message { "cancel", {} };

                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1:  push_text("m"); break;
                          case 2:  push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(observed.background.empty());
        expect(eq(observed.modal, std::string { "m" }));
    };

    "focus groups are one Tab stop and remember their last member"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            int a       = 0;
            int b       = 0;
            int c       = 0;
            int outside = 0;
        };

        struct message
        { std::string action; };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto result
            = app { "sleet-focus-group-test", 320, 240, std::move(theme) }
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message {} }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.action == "a")
                              current.a++;
                          else if (value.action == "b")
                              current.b++;
                          else if (value.action == "c")
                              current.c++;
                          else if (value.action == "outside")
                              current.outside++;
                          return {};
                      })
                  .on_view(
                      [&observed](const state &current)
                      {
                          observed = current;
                          return column()[row()[button("A", "a"), button("B", "b"),
                                                 button("C", "c")]
                                              | focus_group(),
                                          button("Outside", "outside")];
                      })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::activate)
                              return message { input.action };
                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1:  push_key(SDLK_TAB); break;
                          case 2:  push_key(SDLK_RIGHT); break;
                          case 3:  push_key(SDLK_RIGHT); break;
                          case 4:  push_key(SDLK_RETURN); break;
                          case 5:  push_key(SDLK_TAB); break;
                          case 6:  push_key(SDLK_RETURN); break;
                          case 7:  push_key(SDLK_TAB, SDL_KMOD_SHIFT); break;
                          case 8:  push_key(SDLK_RETURN); break;
                          case 9:  push_key(SDLK_LEFT); break;
                          case 10: push_key(SDLK_RETURN); break;
                          case 11: push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(eq(observed.a, 0));
        expect(eq(observed.b, 1));
        expect(eq(observed.c, 2));
        expect(eq(observed.outside, 1));
    };

    "modified activation keys fall through to shortcuts"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            int activations = 0;
            int shortcuts   = 0;
        };

        struct message
        { std::string action; };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto result
            = app { "sleet-modified-activation-test", 320, 240, std::move(theme) }
                  .on_shortcut("Ctrl+Enter", "modified")
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message {} }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.action == "activate")
                              current.activations++;
                          else if (value.action == "modified")
                              current.shortcuts++;
                          return {};
                      })
                  .on_view(
                      [&observed](const state &current)
                      {
                          observed = current;
                          return column()[button("Go", "go")];
                      })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::activate)
                              return message { "activate" };
                          if (input.type == sleet::event::kind::key_down
                              and !input.action.empty())
                              return message { input.action };
                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1:  push_key(SDLK_TAB); break;
                          case 2:  push_key(SDLK_RETURN, SDL_KMOD_CTRL); break;
                          case 3:  push_key(SDLK_RETURN); break;
                          case 4:  push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(eq(observed.shortcuts, 1));
        expect(eq(observed.activations, 1));
    };

    "modal shortcuts are scoped while global shortcuts stay active"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            bool open       = true;
            int  background = 0;
            int  inside     = 0;
            int  global     = 0;
        };

        struct message
        { std::string action; };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto result
            = app { "sleet-modal-shortcut-test", 320, 240, std::move(theme) }
                  .on_shortcut("F4", "global")
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message {} }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.action == "background")
                              current.background++;
                          else if (value.action == "inside")
                              current.inside++;
                          else if (value.action == "global")
                              current.global++;
                          else if (value.action == "cancel")
                              current.open = false;
                          return {};
                      })
                  .on_view(
                      [&observed](const state &current)
                      {
                          observed = current;

                          auto view = column()[box()[label("background")]
                                               | on_shortcut("F2", "background")];
                          if (current.open)
                              view = view[column()[label("dialog")] | modal("cancel")
                                          | on_shortcut("F3", "inside")];
                          return view;
                      })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::key_down
                              and !input.action.empty())
                              return message { input.action };
                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1:  push_key(SDLK_F3); break;
                          case 2:  push_key(SDLK_F2); break;
                          case 3:  push_key(SDLK_F4); break;
                          case 4:  push_key(SDLK_ESCAPE); break;
                          case 5:  push_key(SDLK_F2); break;
                          case 6:  push_key(SDLK_F4); break;
                          case 7:  push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(eq(observed.inside, 1));
        expect(eq(observed.background, 1));
        expect(eq(observed.global, 2));
    };

    "the deepest view shortcut wins and hidden panels fall back"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            bool inner       = true;
            int  outer_fires = 0;
            int  inner_fires = 0;
        };

        struct message
        { std::string action; };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto result
            = app { "sleet-view-shortcut-depth-test", 320, 240, std::move(theme) }
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message {} }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.action == "hide")
                              current.inner = false;
                          else if (value.action == "outer")
                              current.outer_fires++;
                          else if (value.action == "inner")
                              current.inner_fires++;
                          return {};
                      })
                  .on_view(
                      [&observed](const state &current)
                      {
                          observed = current;

                          auto view = column()[label("root")] | on_shortcut("F2", "outer");
                          if (current.inner)
                              view = view[box()[label("inner")] | on_shortcut("F2", "inner")];
                          return view;
                      })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::key_down
                              and !input.action.empty())
                              return message { input.action };
                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1:  push_key(SDLK_F2); break;
                          case 2:  return message { "hide" };
                          case 3:  push_key(SDLK_F2); break;
                          case 4:  push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(eq(observed.inner_fires, 1));
        expect(eq(observed.outer_fires, 1));
    };

    "shortcut repeat is opt-in"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            int once      = 0;
            int repeating = 0;
        };

        struct message
        { std::string action; };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto result
            = app { "sleet-shortcut-repeat-test", 320, 240, std::move(theme) }
                  .on_shortcut(SDLK_F5, SDL_KMOD_NONE, "once")
                  .on_shortcut(SDLK_F6, SDL_KMOD_NONE, "repeating", true)
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message {} }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.action == "once")
                              current.once++;
                          else if (value.action == "repeating")
                              current.repeating++;
                          return {};
                      })
                  .on_view([&observed](const state &current)
                           {
                               observed = current;
                               return column()[label("shortcuts")];
                           })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::key_down
                              and !input.action.empty())
                              return message { input.action };
                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1:  push_key(SDLK_F5, SDL_KMOD_NONE, true); break;
                          case 2:  push_key(SDLK_F5); break;
                          case 3:  push_key(SDLK_F6, SDL_KMOD_NONE, true); break;
                          case 4:  push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(eq(observed.once, 1));
        expect(eq(observed.repeating, 1));
    };
};


const boost::ut::suite frame_reuse_tests = []
{
    using namespace boost::ut;

    "render-only frames reuse the view and layout"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
        };

        struct message
        { int kind = 0; };

        using app = sleet::application<state, message>;

        app *instance = nullptr;
        auto views    = 0;
        auto frame    = 0;

        auto app_object = app { "sleet-frame-reuse-test", 320, 240, std::move(theme) };
        instance        = &app_object;

        auto result = app_object
                          .on_boot([]() -> kei::result<std::pair<state, message>>
                                   { return std::pair { state {}, message {} }; })
                          .on_update(
                              [&instance](state &, const message &value, auto &&) -> kei::result<>
                              {
                                  if (value.kind == 1) instance->notify("toast", 5.F);
                                  return {};
                              })
                          .on_view(
                              [&views](const state &)
                              {
                                  views++;
                                  return column()[button("Hover", "hover") | width(200.F)
                                                  | height(40.F) | transition(0.2F)]
                                       | width(300.F);
                              })
                          .on_subscription(
                              [&frame]() -> std::optional<message>
                              {
                                  switch (++frame)
                                  {
                                  case 1:  push_motion(10.F, 10.F); return message { 1 };
                                  case 8:  push_quit(); break;
                                  default: break;
                                  }

                                  return std::nullopt;
                              })
                          .run();

        expect(eq(result, 0));

        // One view for boot, one for the toast message. The hover transition
        // and toast fade produce several more frames, but all of them should
        // repaint the existing tree instead of rebuilding it.
        expect(views <= 3) << "view calls: " << views;
    };
};


const boost::ut::suite modal_paint_order_tests = []
{
    using namespace boost::ut;

    "the active modal follows paint order, not document order"_test = []
    {
        using namespace sleet::widget;

        // The in-flow modal is declared last, and the higher-z modal first.
        // Paint order still puts the floating modals above the in-flow one,
        // and z-index orders the two floating modals.
        auto tree = to_data(column()[box() | id("high") | floating() | z_index(5) | modal("high"),
                                     box() | id("low") | floating() | z_index(1) | modal("low"),
                                     box() | id("flow") | modal("flow")]);
        expect(static_cast<bool>(tree.finalize()));

        const auto *modal = sleet::_impl::active_modal(tree);
        expect(modal != nullptr);
        if (modal != nullptr) expect(modal->id == widget_id { "high" });
    };

    "equal z-index keeps floating document order"_test = []
    {
        using namespace sleet::widget;

        auto tree = to_data(column()[box() | id("first") | floating() | modal("first"),
                                     box() | id("second") | floating() | modal("second")]);
        expect(static_cast<bool>(tree.finalize()));

        const auto *modal = sleet::_impl::active_modal(tree);
        expect(modal != nullptr);
        if (modal != nullptr) expect(modal->id == widget_id { "second" });
    };
};


const boost::ut::suite modal_hit_tests = []
{
    using namespace boost::ut;

    "a modal blocks clicks outside its surface"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            bool menu_open = true;
            int  under     = 0;
            int  closes    = 0;
        };

        struct message
        { std::string action; };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto result
            = app { "sleet-modal-hit-test", 320, 240, std::move(theme) }
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message {} }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.action == "under")
                              current.under++;
                          else if (value.action == "close_menu")
                          {
                              current.menu_open = false;
                              current.closes++;
                          }
                          return {};
                      })
                  .on_view(
                      [&observed](const state &current)
                      {
                          observed = current;

                          auto view = column();
                          view      = view[button("Under", "under") | width(200.F) | height(40.F)];

                          if (current.menu_open)
                              view = view[box() | modal("close_menu") | on_click("close_menu")
                                          | width(100.F) | height(60.F)];

                          return view;
                      })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::mouse_up
                              and input.button == SDL_BUTTON_LEFT and !input.action.empty())
                              return message { input.action };
                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1:
                              push_mouse(true, 10.F, 10.F);
                              push_mouse(false, 10.F, 10.F);
                              break;
                          case 2:
                              push_mouse(true, 10.F, 10.F);
                              push_mouse(false, 10.F, 10.F);
                              break;
                          case 3:  push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(eq(observed.under, 1));
        expect(eq(observed.closes, 1));
        expect(!observed.menu_open);
    };

    "list rows navigate with arrows and activate with Enter"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            std::string active;
            int         activations = 0;
        };

        struct message
        { std::string action; };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto result
            = app { "sleet-list-test", 320, 240, std::move(theme) }
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message {} }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.action.empty()) return {};
                          current.active = value.action;
                          current.activations++;
                          return {};
                      })
                  .on_view(
                      [&observed](const state &current)
                      {
                          observed = current;

                          auto view = list_view() | width(200.F);
                          view = std::move(view)[list_item("One", "one"), list_item("Two", "two"),
                                                 list_item("Three", "three")];
                          return view;
                      })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::activate and !input.action.empty())
                              return message { input.action };
                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1:  push_key(SDLK_TAB); break;
                          case 2:  push_key(SDLK_END); break;
                          case 3:  push_key(SDLK_HOME); break;
                          case 4:  push_key(SDLK_PAGEDOWN); break;
                          case 5:  push_key(SDLK_PAGEUP); break;
                          case 6:  push_key(SDLK_DOWN); break;
                          case 7:  push_key(SDLK_RETURN); break;
                          case 8:  push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(eq(observed.activations, 1));
        expect(eq(observed.active, std::string { "two" }));
    };

    "tree rows expand, collapse, and move by depth"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            bool        docs_open = false;
            int         toggles   = 0;
            std::string active;
        };

        struct message
        { std::string action; };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto result
            = app { "sleet-tree-test", 320, 240, std::move(theme) }
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message {} }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.action == "toggle_docs")
                          {
                              current.docs_open = !current.docs_open;
                              current.toggles++;
                          }
                          else if (!value.action.empty())
                              current.active = value.action;
                          return {};
                      })
                  .on_view(
                      [&observed](const state &current)
                      {
                          observed = current;

                          auto view = tree_view() | width(240.F) | height(180.F);
                          view = std::move(view)[tree_item("Docs", "docs",
                                                           { .depth         = 0,
                                                             .expanded      = current.docs_open,
                                                             .has_children  = true,
                                                             .toggle_action = "toggle_docs" })];

                          if (current.docs_open)
                              view = std::move(view)[tree_item("Guide", "guide",
                                                               { .depth         = 1,
                                                                 .expanded      = false,
                                                                 .has_children  = false,
                                                                 .toggle_action = {} }),
                                                     tree_item("API", "api",
                                                               { .depth         = 1,
                                                                 .expanded      = false,
                                                                 .has_children  = false,
                                                                 .toggle_action = {} })];
                          return view;
                      })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::activate and !input.action.empty())
                              return message { input.action };
                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1:  push_key(SDLK_TAB); break;
                          case 2:  push_key(SDLK_RIGHT); break;
                          case 3:  push_key(SDLK_RIGHT); break;
                          case 4:  push_key(SDLK_DOWN); break;
                          case 5:  push_key(SDLK_UP); break;
                          case 6:  push_key(SDLK_RETURN); break;
                          case 7:  push_key(SDLK_LEFT); break;
                          case 8:  push_key(SDLK_LEFT); break;
                          case 9:  push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(eq(observed.toggles, 2));
        expect(eq(observed.active, std::string { "guide" }));
        expect(!observed.docs_open);
    };

    "clicking a tree disclosure toggles instead of activating"_test = []
    {
        using namespace sleet::widget;

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty()) return;

        struct state
        {
            bool        docs_open = false;
            int         toggles   = 0;
            std::string active;
        };

        struct message
        { std::string action; };

        using app = sleet::application<state, message>;

        auto observed = state {};
        auto frame    = 0;

        auto result
            = app { "sleet-tree-pointer-test", 320, 240, std::move(theme) }
                  .on_boot([]() -> kei::result<std::pair<state, message>>
                           { return std::pair { state {}, message {} }; })
                  .on_update(
                      [](state &current, const message &value) -> kei::result<>
                      {
                          if (value.action == "toggle_docs")
                          {
                              current.docs_open = !current.docs_open;
                              current.toggles++;
                          }
                          else if (!value.action.empty())
                              current.active = value.action;
                          return {};
                      })
                  .on_view(
                      [&observed](const state &current)
                      {
                          observed = current;

                          auto view = tree_view() | width(200.F) | height(120.F);
                          view = std::move(view)[tree_item("Docs", "docs",
                                                           { .depth         = 0,
                                                             .expanded      = current.docs_open,
                                                             .has_children  = true,
                                                             .toggle_action = "toggle_docs" })];

                          if (current.docs_open)
                              view = std::move(view)[tree_item("Guide", "guide",
                                                               { .depth         = 1,
                                                                 .expanded      = false,
                                                                 .has_children  = false,
                                                                 .toggle_action = {} })];
                          return view;
                      })
                  .on_event(
                      [](const sleet::event &input) -> std::optional<message>
                      {
                          if (input.type == sleet::event::kind::activate and !input.action.empty())
                              return message { input.action };
                          return std::nullopt;
                      })
                  .on_subscription(
                      [&frame]() -> std::optional<message>
                      {
                          switch (++frame)
                          {
                          case 1:
                              push_mouse(true, 15.F, 19.F);
                              push_mouse(false, 15.F, 19.F);
                              break;
                          case 2:
                              push_mouse(true, 100.F, 19.F);
                              push_mouse(false, 100.F, 19.F);
                              break;
                          case 3:  push_quit(); break;
                          default: break;
                          }

                          return std::nullopt;
                      })
                  .run();

        expect(eq(result, 0));
        expect(eq(observed.toggles, 1));
        expect(eq(observed.active, std::string { "docs" }));
    };
};


const boost::ut::suite interaction_target_tests = []
{
    using namespace boost::ut;

    "hover targets stay local to the hovered widget"_test = []
    {
        using namespace sleet::widget;

        auto tree
            = to_data(column()[button("One", "one") | id("one"), button("Two", "two") | id("two")]);
        expect(static_cast<bool>(tree.finalize()));

        const auto progress = sleet::_impl::interaction_targets(tree, widget_id { "one" },
                                                                std::nullopt, std::nullopt, false);

        // The renderer inherits a target's progress in its descendants. If
        // ancestors were marked too, the root would hand hover to every
        // widget in the window.
        expect(eq(progress.size(), std::size_t { 1 }));
        expect(progress.contains(widget_id { "one" }));
        expect(!progress.contains(widget_id { "two" }));

        if (const auto it = progress.find(widget_id { "one" }); it != progress.end())
        {
            expect(it->second.hover == 1.F);
            expect(it->second.press == 0.F);
            expect(it->second.focus == 0.F);
        }
    };
};


auto main() -> int {}
