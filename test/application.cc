#include <cmath>
#include <optional>
#include <string>
#include <utility>

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

        const auto push_key = [](SDL_Keycode key)
        {
            SDL_Event native {};
            native.type    = SDL_EVENT_KEY_DOWN;
            native.key.key = key;
            SDL_PushEvent(&native);
        };

        const auto push_text = [](const char *text)
        {
            SDL_Event native {};
            native.type      = SDL_EVENT_TEXT_INPUT;
            native.text.text = text;
            SDL_PushEvent(&native);
        };

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
                              [&frame, &push_key, &push_text]() -> std::optional<message>
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
};


auto main() -> int {}
