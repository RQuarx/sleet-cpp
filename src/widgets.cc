#include <algorithm>
#include <cmath>
#include <utility>

#include "sleet/widget/modifier.hh"
#include "sleet/widgets.hh"


namespace
{
    using namespace sleet::widget;


    [[nodiscard]]
    auto indicator_style(control_kind kind, bool active, std::source_location origin) -> box_node
    {
        auto result = box(origin) | size(18.F) | control(kind) | checked(active);
        if (kind == control_kind::radio_indicator) result |= radius(9.F);
        return result;
    }


    [[nodiscard]]
    auto option_label(std::string text, std::source_location origin) -> box_node
    {
        return box(origin)[label(std::move(text), origin) | hexpand() | align_self(align::center)]
             | padding(5.F, 10.F) | height(32.F) | width(sizing::fill()) | radius(4.F);
    }


    [[nodiscard]]
    auto make_combo_box(std::string                   value,
                        std::vector<std::string>      options,
                        const std::vector<widget_id> *option_ids,
                        std::string                   action,
                        std::string                   hint,
                        std::source_location          origin) -> combo_box_node
    {
        const auto display = value.empty() ? hint : value;

        // The renderer draws the disclosure chevron; the blank box keeps the
        // label from running underneath it.
        auto header_content
            = row(origin)[label(display, origin) | hexpand() | align_self(align::center),
                          box(origin) | width(8.F) | align_self(align::center)]
            | gap(8.F) | width(sizing::fill()) | valign(align::center);

        auto header = box(origin)[std::move(header_content)] | padding(8.F, 10.F) | height(36.F)
                    | width(sizing::fill()) | control(control_kind::combo_box) | focusable()
                    | on_click(action) | text_of(value) | transition(0.12F);

        auto result = column(origin);
        result     |= width(sizing::fill());
        result     |= gap(2.F);
        result      = std::move(result)[std::move(header)];

        // The option list is one floating box: it is the root of the popup
        // subtree and keeps normal column layout for its rows.
        auto options_column = column(origin);
        options_column     |= width(sizing::fill());
        options_column     |= gap(2.F);

        for (std::size_t i = 0; i < options.size(); i++)
        {
            auto option = option_label(options[i], origin) | control(control_kind::combo_option)
                        | focusable() | on_click(action) | text_of(options[i]) | radius(4.F)
                        | transition(0.08F);

            if (option_ids != nullptr and i < option_ids->size()) option |= key((*option_ids)[i]);

            options_column = std::move(options_column)[std::move(option)];
        }

        auto popup = floating_box(anchor_point::top_left, 100, origin) | width(sizing::fill())
                   | placement(0.F, 38.F) | padding(2.F) | visible(false)
                   | control(control_kind::combo_popup);
        popup      = std::move(popup)[std::move(options_column)];

        result = std::move(result)[std::move(popup)];

        return result;
    }
}


auto sleet::widget::box(std::source_location origin) -> box_node { return { origin }; }
auto sleet::widget::floating_box(anchor_point at, int stacking, std::source_location origin)
    -> floating_node
{ return floating_node { origin, at, stacking }; }
auto sleet::widget::column(std::source_location origin) -> column_node { return { origin }; }
auto sleet::widget::row(std::source_location origin) -> row_node { return { origin }; }
auto sleet::widget::label(std::string text, std::source_location origin) -> label_node
{ return label_node { std::move(text), origin }; }


auto sleet::widget::button(std::string          text,
                           std::string          action,
                           button_variant       appearance,
                           std::source_location origin) -> button_node
{
    auto content = label(std::move(text), origin) | text_size(15.F) | halign(align::center)
                 | valign(align::center) | hexpand();

    return box(origin)[std::move(content)] | padding(8.F, 14.F) | height(36.F)
         | halign(align::center) | valign(align::center) | on_click(std::move(action)) | focusable()
         | variant(appearance) | transition(0.12F);
}


auto sleet::widget::button(node_data            content,
                           std::string          action,
                           button_variant       appearance,
                           std::source_location origin) -> button_node
{
    return box(origin)[std::move(content)] | padding(8.F, 14.F) | height(36.F)
         | halign(align::center) | valign(align::center) | on_click(std::move(action)) | focusable()
         | variant(appearance) | transition(0.12F);
}


auto sleet::widget::text_input(std::string          value,
                               std::string          action,
                               std::string          hint,
                               std::source_location origin) -> text_input_node
{
    return label_node { std::move(value), origin } | padding(8.F, 10.F) | width(sizing::fill())
         | height(36.F) | control(control_kind::text_input) | focusable()
         | on_click(std::move(action)) | placeholder(std::move(hint)) | transition(0.12F);
}


auto sleet::widget::text_area(std::string          value,
                              std::string          action,
                              std::string          hint,
                              std::source_location origin) -> text_input_node
{
    return label_node { std::move(value), origin } | padding(8.F, 10.F) | width(sizing::fill())
         | height(120.F) | control(control_kind::text_input) | focusable()
         | on_click(std::move(action)) | placeholder(std::move(hint))
         | text_wrap(text_wrap_mode::wrap) | multiline() | transition(0.12F);
}


auto sleet::widget::combo_box(std::string              value,
                              std::vector<std::string> options,
                              std::string              action,
                              std::string              hint,
                              std::source_location     origin) -> combo_box_node
{
    return make_combo_box(std::move(value), std::move(options), nullptr, std::move(action),
                          std::move(hint), origin);
}


auto sleet::widget::combo_box(std::string              value,
                              std::vector<std::string> options,
                              std::vector<widget_id>   option_ids,
                              std::string              action,
                              std::string              hint,
                              std::source_location     origin) -> combo_box_node
{
    return make_combo_box(std::move(value), std::move(options), &option_ids, std::move(action),
                          std::move(hint), origin);
}


auto sleet::widget::checkbox(std::string          text,
                             bool                 is_checked,
                             std::string          action,
                             std::source_location origin) -> checkbox_node
{
    auto content
        = row(origin)[indicator_style(control_kind::checkbox_indicator, is_checked, origin),
                      label(std::move(text), origin) | valign(align::center)]
        | gap(8.F) | valign(align::center);

    return box(origin)[std::move(content)] | padding(4.F) | halign(align::start)
         | valign(align::center) | on_click(std::move(action)) | focusable() | transition(0.12F);
}


auto sleet::widget::toggle(std::string          text,
                           bool                 is_checked,
                           std::string          action,
                           std::source_location origin) -> toggle_node
{
    auto thumb = box(origin) | size(18.F) | tone_of(tone::accent_fg) | radius(9.F);
    auto track = box(origin)[std::move(thumb)] | size(38.F, 22.F) | padding(2.F)
               | tone_of(is_checked ? tone::accent : tone::field) | radius(11.F)
               | halign(is_checked ? align::end : align::start) | valign(align::center);

    auto content
        = row(origin)[std::move(track), label(std::move(text), origin) | align_self(align::center)]
        | gap(8.F) | valign(align::center);

    return box(origin)[std::move(content)] | padding(4.F) | halign(align::start)
         | valign(align::center) | on_click(std::move(action)) | focusable() | transition(0.12F);
}


auto sleet::widget::radio_button(std::string          text,
                                 bool                 is_selected,
                                 std::string          action,
                                 std::source_location origin) -> radio_node
{
    auto content = row(origin)[indicator_style(control_kind::radio_indicator, is_selected, origin),
                               label(std::move(text), origin) | valign(align::center)]
                 | gap(8.F) | valign(align::center);

    return box(origin)[std::move(content)] | padding(4.F) | halign(align::start)
         | valign(align::center) | on_click(std::move(action)) | focusable() | transition(0.12F);
}


auto sleet::widget::slider(float value, std::string action, std::source_location origin)
    -> slider_node
{
    value = std::clamp(value, 0.F, 1.F);

    // The renderer draws the track, fill, and thumb from the control value, so
    // the slider stays one clean shape instead of three adjacent boxes.
    return box(origin) | width(sizing::fill()) | height(20.F) | control(control_kind::slider)
         | focusable() | on_click(std::move(action)) | control_value(value) | transition(0.12F);
}


auto sleet::widget::progress_bar(float value, float bar_height, std::source_location origin)
    -> progress_node
{
    value = std::clamp(value, 0.F, 1.F);

    auto fill = box(origin) | width(sizing::grow()) | flex(value) | height(sizing::fill())
              | tone_of(tone::accent);
    auto rest = box(origin) | width(sizing::grow()) | flex(1.F - value) | height(sizing::fill())
              | tone_of(tone::field);

    return row(origin)[std::move(fill), std::move(rest)] | width(sizing::fill())
         | height(std::max(1.F, bar_height)) | gap(0.F);
}


auto sleet::widget::separator(std::source_location origin) -> separator_node
{ return box(origin) | width(sizing::fill()) | height(1.F) | tone_of(tone::border); }


auto sleet::widget::spacer(std::source_location origin) -> spacer_node
{ return box(origin) | hexpand() | vexpand(); }


auto sleet::widget::badge(std::string text, std::source_location origin) -> badge_node
{
    return label_node { std::move(text), origin } | padding(2.F, 8.F) | text_size(12.F)
         | tone_of(tone::accent) | radius(4.F);
}


auto sleet::widget::menu_item(std::string          text,
                              std::string          action,
                              bool                 selected,
                              std::source_location origin) -> button_node
{
    auto marker = box(origin) | width(16.F) | align_self(align::center);
    if (selected)
        marker = std::move(marker)[box(origin) | size(6.F) | tone_of(tone::accent) | radius(3.F)
                                   | align_self(align::center)];

    auto row_widget
        = row(origin)[std::move(marker), label(std::move(text), origin) | hexpand()
                                             | text_size(14.F) | align_self(align::center)];

    return box(origin)[std::move(row_widget)] | padding(6.F, 10.F) | width(sizing::fill())
         | on_click(std::move(action)) | focusable() | variant(button_variant::ghost)
         | transition(0.1F) | radius(4.F);
}


auto sleet::widget::menu(std::source_location origin) -> column_node
{
    return column(origin) | floating() | anchor_at(anchor_point::top_left) | stacking(200)
         | background_of(tone::field) | border_of(tone::border) | padding(4.F) | gap(2.F)
         | width(200.F);
}


namespace
{
    [[nodiscard]]
    auto column_weight(float value) noexcept -> float
    { return std::isfinite(value) ? std::max(0.01F, value) : 1.F; }
}


auto sleet::widget::list_view(std::source_location origin) -> list_view_node
{
    return column(origin) | width(sizing::fill()) | height(240.F) | control(control_kind::list_view)
         | focus_group() | padding(4.F) | gap(2.F) | background_of(tone::field)
         | border_of(tone::border) | radius(6.F) | clip() | scrollable();
}


auto sleet::widget::list_item(node_data            content,
                              std::string          action,
                              bool                 is_selected,
                              std::source_location origin) -> list_item_node
{
    return box(origin)[std::move(content)] | width(sizing::fill()) | height(32.F)
         | padding(6.F, 10.F) | control(control_kind::list_item) | focusable()
         | on_click(std::move(action)) | selected(is_selected) | radius(4.F) | transition(0.1F);
}


auto sleet::widget::list_item(std::string          text,
                              std::string          action,
                              bool                 is_selected,
                              std::source_location origin) -> list_item_node
{
    auto content = label(text, origin) | hexpand() | align_self(align::center)
                 | text_wrap(text_wrap_mode::no_wrap);
    auto result  = list_item(to_data(std::move(content)), std::move(action), is_selected, origin);
    result      |= text_of(std::move(text));
    return result;
}


auto sleet::widget::table_view(std::source_location origin) -> table_view_node
{
    return column(origin) | width(sizing::fill()) | height(240.F) | control(control_kind::list_view)
         | focus_group() | padding(1.F) | gap(1.F) | background_of(tone::border)
         | border_of(tone::border) | radius(6.F) | clip() | scrollable();
}


auto sleet::widget::table_row(std::string action, bool is_selected, std::source_location origin)
    -> table_row_node
{
    return row(origin) | width(sizing::fill()) | height(32.F) | background_of(tone::base)
         | control(control_kind::list_item) | focusable() | on_click(std::move(action))
         | selected(is_selected) | radius(0.F) | transition(0.1F);
}


auto sleet::widget::table_header(std::source_location origin) -> table_row_node
{
    return row(origin) | width(sizing::fill()) | height(32.F) | background_of(tone::field)
         | radius(0.F);
}


auto sleet::widget::table_cell(std::string text, float weight, std::source_location origin)
    -> table_cell_node
{
    return box(origin)[label(std::move(text), origin) | hexpand() | align_self(align::center)
                       | text_wrap(text_wrap_mode::no_wrap)]
         | width(sizing::grow()) | flex(column_weight(weight)) | padding(6.F, 8.F) | clip();
}


auto sleet::widget::table_header_cell(std::string text, float weight, std::source_location origin)
    -> table_cell_node
{
    return box(origin)[label(std::move(text), origin) | hexpand() | align_self(align::center)
                       | text_wrap(text_wrap_mode::no_wrap) | text_weight(font_weight::semibold)]
         | width(sizing::grow()) | flex(column_weight(weight)) | padding(6.F, 8.F) | clip();
}


auto sleet::widget::tree_view(std::source_location origin) -> tree_view_node
{
    return column(origin) | width(sizing::fill()) | height(280.F) | control(control_kind::tree_view)
         | focus_group() | padding(4.F) | gap(1.F) | background_of(tone::field)
         | border_of(tone::border) | radius(6.F) | clip() | scrollable();
}


auto sleet::widget::tree_item(node_data            content,
                              std::string          action,
                              tree_item_options    options,
                              std::source_location origin) -> tree_item_node
{
    const auto row_depth = std::max(0, options.depth);
    const auto indent
        = tree_row_inset + (static_cast<float>(row_depth) * tree_indent_width) + tree_chevron_width;

    return box(origin)[std::move(content)] | width(sizing::fill()) | height(30.F)
         | padding(math::edge_insets { 5.F, 10.F, 5.F, indent }) | control(control_kind::tree_item)
         | focusable() | on_click(std::move(action)) | depth(row_depth) | expanded(options.expanded)
         | has_children(options.has_children) | on_toggle(std::move(options.toggle_action))
         | radius(4.F) | transition(0.1F);
}


auto sleet::widget::tree_item(std::string          text,
                              std::string          action,
                              tree_item_options    options,
                              std::source_location origin) -> tree_item_node
{
    auto content = label(text, origin) | hexpand() | align_self(align::center)
                 | text_wrap(text_wrap_mode::no_wrap);
    auto result
        = tree_item(to_data(std::move(content)), std::move(action), std::move(options), origin);
    result |= text_of(std::move(text));
    return result;
}
