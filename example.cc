#include <cstdint>
#include <cstdio>
#include <exception>
#include <format>
#include <optional>
#include <string>
#include <utility>

#include <SDL3/SDL_mouse.h>
#include <sleet/sleet.hh>


namespace
{
    using namespace sleet;
    using namespace sleet::literals;
    using namespace sleet::widget;

    struct state
    {
        int         count = 0;
        std::string name;
        std::string color         = "Blue";
        std::string density       = "Comfortable";
        bool        show_details  = true;
        bool        notifications = true;
        bool        autosave      = false;
        float       volume        = 0.4F;
    };


    struct message
    {
        enum class kind : std::uint8_t
        {
            none,
            increment,
            decrement,
            reset,
            clear,
            boost,
            name_changed,
            color_changed,
            density_changed,
            toggle_details,
            toggle_notifications,
            toggle_autosave,
            volume_changed,
        };


        message(kind type = kind::none, std::string text = {}, float value = 0.F)
            : type { type }, text { std::move(text) }, value { value }
        {
        }


        kind        type = kind::none;
        std::string text;
        float       value = 0.F;
    };


    using app = application<state, message>;


    [[nodiscard]]
    auto muted(std::string text) -> label_node
    { return label(std::move(text)) | text_role(font_role::caption) | text_color(0x98A2B0_rgb); }


    [[nodiscard]]
    auto section_title(std::string text) -> label_node
    {
        return label(std::move(text)) | text_role(font_role::heading) | text_size(16.F)
             | text_color(0xE6E8EB_rgb);
    }


    [[nodiscard]]
    auto card(column_node content) -> box_node
    {
        return box()[std::move(content)] | width(sizing::fill()) | padding(14.F)
             | background(0x1D2026_rgb) | border(0x2E333C_rgb);
    }


    [[nodiscard]]
    auto stat_line(std::string key, std::string value) -> row_node
    {
        return row()[muted(std::move(key)) | hexpand(),
                     label(std::move(value)) | text_color(0xE6E8EB_rgb)]
             | gap(12.F) | valign(align::center);
    }


    [[nodiscard]]
    auto button_action(const std::string &action) -> std::optional<message>
    {
        if (action == "increment") return message { message::kind::increment };
        if (action == "decrement") return message { message::kind::decrement };
        if (action == "reset") return message { message::kind::reset };
        if (action == "clear") return message { message::kind::clear };
        if (action == "boost") return message { message::kind::boost };
        if (action == "details") return message { message::kind::toggle_details };
        if (action == "notifications") return message { message::kind::toggle_notifications };
        if (action == "autosave") return message { message::kind::toggle_autosave };
        return std::nullopt;
    }


    [[nodiscard]]
    auto density_action(const std::string &action) -> std::optional<message>
    {
        if (action == "density_compact")
            return message { message::kind::density_changed, "Compact" };
        if (action == "density_comfortable")
            return message { message::kind::density_changed, "Comfortable" };
        if (action == "density_spacious")
            return message { message::kind::density_changed, "Spacious" };
        return std::nullopt;
    }


    [[nodiscard]]
    auto view(const state &current) -> column_node
    {
        auto header = row()[column()[label("Sleet widget gallery") | text_size(28.F)
                                         | text_role(font_role::heading) | text_color(0xF3F5F8_rgb),
                                     muted("Every built-in widget in one interactive window.")]
                                | gap(2.F),
                            spacer(), badge("v0.1")]
                    | gap(12.F) | valign(align::center);


        auto input_card
            = card(column()[section_title("Text input"),
                            text_input(current.name, "name", "Type your name..."),
                            muted("Enter submits. Arrows, Home/End, Ctrl+A and Ctrl+C/X/V work.")]
                   | gap(8.F));


        auto selection_card = card(
            column()[section_title("Combo box, checkbox, toggle, radio"), muted("Combo box"),
                     combo_box(current.color, { "Red", "Green", "Blue", "Purple" }, "color",
                               "Choose a color..."),
                     row()[checkbox("Show details", current.show_details, "details"),
                           checkbox("Auto-save", current.autosave, "autosave")]
                         | gap(16.F),
                     toggle("Notifications", current.notifications, "notifications"),
                     muted("Radio buttons"),
                     row()[radio_button("Compact", current.density == "Compact", "density_compact"),
                           radio_button("Comfortable", current.density == "Comfortable",
                                        "density_comfortable"),
                           radio_button("Spacious", current.density == "Spacious",
                                        "density_spacious")]
                         | gap(10.F)]
            | gap(8.F));


        auto slider_card
            = card(column()[section_title("Slider and progress bar"),
                            row()[label("Volume") | text_color(0xE6E8EB_rgb),
                                  slider(current.volume, "volume") | hexpand(),
                                  badge(std::format("{:.0f}%", current.volume * 100.F))]
                                | gap(10.F) | valign(align::center),
                            progress_bar(current.volume, 10.F),
                            muted("Drag the slider or focus it and use the arrow keys.")]
                   | gap(10.F));


        auto ghost_label = current.show_details ? "Hide details" : "Show details";
        auto boost_content
            = to_data(row()[label("Boost") | text_color(0xF3F5F8_rgb), badge("+10")] | gap(6.F));
        auto buttons_card
            = card(column()[section_title("Buttons (all variants)"),
                            row()[button("Decrease", "decrement"),
                                  button("Increase", "increment", button_variant::primary),
                                  button(ghost_label, "details", button_variant::ghost)]
                                | gap(8.F),
                            row()[button("Clear name", "clear", button_variant::danger),
                                  button("Reset all", "reset"), button(boost_content, "boost")]
                                | gap(8.F)]
                   | gap(10.F));


        auto status_card = card(
            column()[row()[box() | size(10.F) | tone_of(tone::accent), section_title("State")]
                         | gap(8.F) | valign(align::center),
                     stat_line("Name", current.name.empty() ? "(empty)" : current.name),
                     stat_line("Color", current.color), stat_line("Density", current.density),
                     stat_line("Details", current.show_details ? "shown" : "hidden"),
                     stat_line("Notifications", current.notifications ? "on" : "off"),
                     stat_line("Auto-save", current.autosave ? "on" : "off"),
                     stat_line("Count", std::format("{}", current.count)), separator(),
                     label(std::format("Volume {:.0f}%", current.volume * 100.F)) | text_size(20.F)
                         | text_color(0xE6E8EB_rgb)]
            | gap(6.F));


        auto styles_card = card(
            column()[section_title("Text styles"),
                     label("Heading") | text_role(font_role::heading) | text_size(20.F),
                     row()[muted("Caption"), label("Bold") | text_weight(font_weight::bold),
                           label("Underlined") | text_decoration(text_attribute::flag::underline)]
                         | gap(12.F),
                     label("Monospace") | text_role(font_role::code)]
            | gap(8.F));


        auto left
            = column()[std::move(input_card), std::move(selection_card), std::move(slider_card)]
            | width(sizing::grow()) | flex(1.F) | gap(14.F);
        auto right
            = column()[std::move(buttons_card), std::move(status_card), std::move(styles_card)]
            | width(sizing::grow()) | flex(1.F) | gap(14.F);


        auto footer = row()[muted("Tab moves focus. Click a combo header to open it. "
                                  "Clicking outside closes it."),
                            spacer(), badge("interactive")]
                    | gap(12.F) | valign(align::center);


        return column()[std::move(header), separator(),
                        row()[std::move(left), std::move(right)] | gap(16.F)
                            | width(sizing::fill()),
                        separator(), std::move(footer)]
             | padding(20.F) | gap(14.F) | size(sizing::fill()) | background(0x14161A_rgb);
    }
}


auto main() -> int
{
    try
    {
        return app { "Sleet widget gallery", 1040, 820 }
            .on_boot([]() -> kei::result<std::pair<state, message>>
                     { return std::pair { state {}, message {} }; })
            .on_update(
                [](state &current, const message &value) -> kei::result<>
                {
                    switch (value.type)
                    {
                    case message::kind::none:            break;
                    case message::kind::increment:       current.count++; break;
                    case message::kind::decrement:       current.count--; break;
                    case message::kind::reset:           current = state {}; break;
                    case message::kind::clear:           current.name.clear(); break;
                    case message::kind::boost:           current.count += 10; break;
                    case message::kind::name_changed:    current.name = value.text; break;
                    case message::kind::color_changed:   current.color = value.text; break;
                    case message::kind::density_changed: current.density = value.text; break;
                    case message::kind::toggle_details:
                        current.show_details = !current.show_details;
                        break;
                    case message::kind::toggle_notifications:
                        current.notifications = !current.notifications;
                        break;
                    case message::kind::toggle_autosave:
                        current.autosave = !current.autosave;
                        break;
                    case message::kind::volume_changed: current.volume = value.value; break;
                    }
                    return {};
                })
            .on_view(view)
            .on_event(
                [](const event &input) -> std::optional<message>
                {
                    switch (input.type)
                    {
                    case event::kind::mouse_up:
                        if (input.button != SDL_BUTTON_LEFT) return std::nullopt;
                        [[fallthrough]];
                    case event::kind::activate:
                        if (const auto result = button_action(input.action)) return result;
                        return density_action(input.action);

                    case event::kind::text_edit:
                        if (input.action == "name")
                            return message { message::kind::name_changed, input.text };
                        return std::nullopt;

                    case event::kind::selection_changed:
                        if (input.action == "color")
                            return message { message::kind::color_changed, input.text };
                        return std::nullopt;

                    case event::kind::slider_changed:
                        if (input.action == "volume")
                            return message { message::kind::volume_changed, {}, input.value };
                        return std::nullopt;

                    default: return std::nullopt;
                    }
                })
            .run();
    }
    catch (const std::exception &failure)
    {
        std::fprintf(stderr, "sleet: %s\n", failure.what());
        return 1;
    }
}
