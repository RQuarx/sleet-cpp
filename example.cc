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
        std::string picked;
        std::string menu_choice;
        bool        menu_open   = false;
        bool        dialog_open = false;
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
            scroll_pick,
            menu_toggle,
            menu_close,
            menu_pick,
            dialog_open,
            dialog_cancel,
            dialog_save,
            toast,
        };


        message(kind message_kind = kind::none, std::string message_text = {},
                float message_value = 0.F)
            : type { message_kind }, text { std::move(message_text) }, value { message_value }
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
        content |= width(sizing::fill());

        return box()[std::move(content)] | width(sizing::fill()) | padding(14.F)
             | background(0x1D2026_rgb) | border(0x2E333C_rgb);
    }


    [[nodiscard]]
    auto stat_line(std::string key, std::string value) -> row_node
    {
        return row()[muted(std::move(key)) | hexpand(),
                     label(std::move(value)) | text_color(0xE6E8EB_rgb)]
             | gap(12.F) | width(sizing::fill()) | valign(align::center);
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
        if (action.starts_with("scroll_"))
            return message { message::kind::scroll_pick, action.substr(7) };
        if (action == "menu_toggle") return message { message::kind::menu_toggle };
        if (action == "menu_close") return message { message::kind::menu_close };
        if (action == "menu_duplicate" or action == "menu_archive" or action == "menu_delete")
            return message { message::kind::menu_pick, action.substr(5) };
        if (action == "dialog_open") return message { message::kind::dialog_open };
        if (action == "dialog_cancel") return message { message::kind::dialog_cancel };
        if (action == "dialog_save") return message { message::kind::dialog_save };
        if (action == "toast") return message { message::kind::toast };
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
    auto view(const state &current) -> box_node
    {
        auto header = row()[column()[label("Sleet widget gallery") | text_size(28.F)
                                         | text_role(font_role::heading) | text_color(0xF3F5F8_rgb),
                                     muted("Every built-in widget in one interactive window.")]
                                | gap(2.F),
                            spacer(), badge("v1.3.2")]
                    | gap(12.F) | width(sizing::fill()) | valign(align::center);


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
                                | gap(10.F) | width(sizing::fill()) | valign(align::center),
                            progress_bar(current.volume, 10.F),
                            muted("Drag the slider or focus it and use the arrow keys.")]
                   | gap(10.F));


        auto scroll_rows = column() | gap(4.F);
        for (auto i = 1; i <= 8; i++)
            scroll_rows = scroll_rows[button(std::format("Row {}", i), std::format("scroll_{}", i))
                                      | width(sizing::fill())];

        auto scroll_card = card(
            column()[row()[section_title("Scrollable list"), spacer(),
                           badge(current.picked.empty() ? "scroll me"
                                                        : std::format("Row {}", current.picked))]
                         | gap(8.F) | width(sizing::fill()) | valign(align::center),
                     scroll_rows | id("gallery-scroll") | height(116.F) | width(sizing::fill())
                         | clip() | scrollable(),
                     muted("Wheel to scroll. Tab into an off-screen row to reveal it.")]
            | gap(8.F));


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
                     stat_line("Picked",
                               current.picked.empty() ? "(none)" : "Row " + current.picked),
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


        auto menu_holder = column()[button("Actions", "menu_toggle") | id("menu-trigger")
                                    | tooltip("Open the quick actions menu")]
                         | width(sizing::fit());

        if (current.menu_open)
        {
            auto popup  = menu() | id("gallery-menu") | placement(0.F, 40.F) | modal("menu_close");
            popup       = popup[menu_item("Duplicate", "menu_duplicate"),
                                menu_item("Archive", "menu_archive", true), separator(),
                                menu_item("Delete", "menu_delete")];
            menu_holder = menu_holder[std::move(popup)];
        }

        auto overlay_card = card(
            column()[section_title("Tooltips, menus, and dialogs"),
                     muted("Hover for a tooltip. Menus and dialogs trap focus; Escape cancels."),
                     row()[std::move(menu_holder),
                           button("Edit profile", "dialog_open", button_variant::primary)
                               | id("dialog-trigger"),
                           button("Toast", "toast") | id("toast-trigger")]
                         | gap(8.F) | valign(align::center),
                     muted(current.menu_choice.empty() ? "Menu: none"
                                                       : "Menu: " + current.menu_choice)]
            | gap(10.F));


        auto left  = column()[std::move(input_card), std::move(selection_card),
                              std::move(scroll_card), std::move(styles_card)]
                   | width(sizing::grow()) | flex(1.F) | gap(14.F);
        auto right = column()[std::move(buttons_card), std::move(slider_card),
                              std::move(overlay_card), std::move(status_card)]
                   | width(sizing::grow()) | flex(1.F) | gap(14.F);


        auto footer = row()[muted("Tab moves focus. Click a combo header to open it. "
                                  "Clicking outside closes it."),
                            spacer(), badge("interactive")]
                    | gap(12.F) | width(sizing::fill()) | valign(align::center);


        auto content
            = column()[std::move(header), separator(),
                       row()[std::move(left), std::move(right)] | gap(16.F) | width(sizing::fill()),
                       separator(), std::move(footer)]
            | padding(20.F) | gap(14.F) | size(sizing::fill());

        auto root = box()[std::move(content)] | size(sizing::fill()) | background(0x14161A_rgb);

        if (current.dialog_open)
        {
            auto dialog = column()[label("Save changes?") | text_role(font_role::heading)
                                       | text_size(20.F) | text_color(0xF3F5F8_rgb),
                                   muted("This dialog traps Tab focus. Escape or a scrim click "
                                         "cancels."),
                                   row()[button("Cancel", "dialog_cancel"), spacer(),
                                         button("Save", "dialog_save", button_variant::primary)]
                                       | gap(8.F) | width(sizing::fill())]
                        | gap(12.F) | padding(18.F) | width(380.F) | background_of(tone::field)
                        | border_of(tone::border) | align_self(align::center) | id("dialog-panel");

            auto overlay = floating_box(anchor_point::center, 400) | id("dialog-overlay")
                         | backdrop() | modal("dialog_cancel");
            overlay      = overlay[std::move(dialog)];
            root         = root[std::move(overlay)];
        }

        return root;
    }
}


auto main() -> int
{
    try
    {
        auto instance = app { "Sleet widget gallery", 1040, 1000 };

        instance
            .on_boot([]() -> kei::result<std::pair<state, message>>
                     { return std::pair { state {}, message {} }; })
            .on_update(
                [&instance](state &current, const message &value) -> kei::result<>
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
                    case message::kind::scroll_pick:    current.picked = value.text; break;
                    case message::kind::menu_toggle: current.menu_open = !current.menu_open; break;
                    case message::kind::menu_close:  current.menu_open = false; break;
                    case message::kind::menu_pick:
                        current.menu_open   = false;
                        current.menu_choice = value.text;
                        break;
                    case message::kind::dialog_open:
                        current.dialog_open = true;
                        current.menu_open   = false;
                        break;
                    case message::kind::dialog_cancel: current.dialog_open = false; break;
                    case message::kind::dialog_save:
                        current.dialog_open = false;
                        instance.notify("Profile saved", 2.5F);
                        break;
                    case message::kind::toast:
                        instance.notify("This is a toast notification", 3.F);
                        break;
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

                    case event::kind::key_down:
                        if (input.action == "dialog_cancel")
                            return message { message::kind::dialog_cancel };
                        if (input.action == "menu_close")
                            return message { message::kind::menu_close };
                        return std::nullopt;

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
                });

        return instance.run();
    }
    catch (const std::exception &failure)
    {
        std::fprintf(stderr, "sleet: %s\n", failure.what());
        return 1;
    }
}
