#include <algorithm>
#include <utility>

#include "sleet/widget/modifier.hh"
#include "sleet/widgets.hh"


namespace
{
    using namespace sleet::widget;


    [[nodiscard]]
    auto indicator_style(control_kind kind, bool active, std::source_location origin) -> box_node
    { return box(origin) | size(18.F) | control(kind) | checked(active); }


    [[nodiscard]]
    auto option_label(std::string text, std::source_location origin) -> box_node
    {
        return box(origin)[label(std::move(text), origin) | hexpand() | valign(align::center)]
             | padding(8.F, 10.F) | height(32.F) | width(sizing::fill());
    }
}


auto sleet::widget::box(std::source_location origin) -> box_node { return { origin }; }
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
         | variant(appearance);
}


auto sleet::widget::button(node_data            content,
                           std::string          action,
                           button_variant       appearance,
                           std::source_location origin) -> button_node
{
    return box(origin)[std::move(content)] | padding(8.F, 14.F) | height(36.F)
         | halign(align::center) | valign(align::center) | on_click(std::move(action)) | focusable()
         | variant(appearance);
}


auto sleet::widget::text_input(std::string          value,
                               std::string          action,
                               std::string          hint,
                               std::source_location origin) -> text_input_node
{
    return label_node { std::move(value), origin } | padding(8.F, 10.F) | width(sizing::fill())
         | height(36.F) | control(control_kind::text_input) | focusable()
         | on_click(std::move(action)) | placeholder(std::move(hint));
}


auto sleet::widget::combo_box(std::string              value,
                              std::vector<std::string> options,
                              std::string              action,
                              std::string              hint,
                              std::source_location     origin) -> combo_box_node
{
    const auto display = value.empty() ? hint : value;

    auto header_content
        = row(origin)[label(display, origin) | hexpand() | valign(align::center),
                      label("\xE2\x96\xBE", origin) | valign(align::center) | text_size(12.F)]
        | gap(8.F);

    auto header = box(origin)[std::move(header_content)] | padding(8.F, 10.F) | height(36.F)
                | width(sizing::fill()) | control(control_kind::combo_box) | focusable()
                | on_click(action) | text_of(value);

    auto result = column(origin);
    result     |= width(sizing::fill());
    result     |= gap(2.F);
    result      = result[std::move(header)];

    for (auto &option : options)
        result = result[option_label(option, origin) | control(control_kind::combo_option)
                        | focusable() | on_click(action) | text_of(option) | visible(false)];

    return result;
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
         | valign(align::center) | on_click(std::move(action)) | focusable();
}


auto sleet::widget::toggle(std::string          text,
                           bool                 is_checked,
                           std::string          action,
                           std::source_location origin) -> toggle_node
{
    auto track = box(origin)[box(origin) | size(18.F) | tone_of(tone::accent_fg)] | size(38.F, 22.F)
               | padding(2.F) | tone_of(is_checked ? tone::accent : tone::field)
               | halign(is_checked ? align::end : align::start) | valign(align::center);

    auto content
        = row(origin)[std::move(track), label(std::move(text), origin) | valign(align::center)]
        | gap(8.F) | valign(align::center);

    return box(origin)[std::move(content)] | padding(4.F) | halign(align::start)
         | valign(align::center) | on_click(std::move(action)) | focusable();
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
         | valign(align::center) | on_click(std::move(action)) | focusable();
}


auto sleet::widget::slider(float value, std::string action, std::source_location origin)
    -> slider_node
{
    value = std::clamp(value, 0.F, 1.F);

    auto left  = box(origin) | width(sizing::grow()) | flex(value) | height(sizing::fill())
               | tone_of(tone::accent);
    auto thumb = box(origin) | width(16.F) | height(sizing::fill()) | tone_of(tone::accent_fg);
    auto right = box(origin) | width(sizing::grow()) | flex(1.F - value) | height(sizing::fill())
               | tone_of(tone::field);

    return box(origin)[row(origin)[std::move(left), std::move(thumb), std::move(right)] | gap(0.F)
                       | width(sizing::fill()) | height(sizing::fill())]
         | width(sizing::fill()) | height(20.F) | control(control_kind::slider) | focusable()
         | on_click(std::move(action)) | control_value(value);
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
         | tone_of(tone::accent);
}
