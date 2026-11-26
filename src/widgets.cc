#include <algorithm>
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

        auto header_content
            = row(origin)[label(display, origin) | hexpand() | align_self(align::center),
                          label("\xE2\x96\xBE", origin) | align_self(align::center)
                              | text_size(12.F)]
            | gap(8.F) | valign(align::center);

        auto header = box(origin)[std::move(header_content)] | padding(8.F, 10.F) | height(36.F)
                    | width(sizing::fill()) | control(control_kind::combo_box) | focusable()
                    | on_click(action) | text_of(value);

        auto result = column(origin);
        result     |= width(sizing::fill());
        result     |= gap(2.F);
        result      = result[std::move(header)];

        // Options float below the header instead of growing the column, so
        // opening a combo box never pushes the rest of the layout down.
        auto offset = 38.F;
        for (std::size_t i = 0; i < options.size(); i++)
        {
            auto option = option_label(options[i], origin) | control(control_kind::combo_option)
                        | focusable() | on_click(action) | text_of(options[i]) | visible(false)
                        | floating() | placement(0.F, offset) | z_index(100) | radius(4.F);

            if (option_ids != nullptr and i < option_ids->size()) option |= key((*option_ids)[i]);

            result  = result[std::move(option)];
            offset += 34.F;
        }

        return result;
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
         | valign(align::center) | on_click(std::move(action)) | focusable();
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
    auto thumb = box(origin) | width(16.F) | height(sizing::fill()) | tone_of(tone::accent_fg)
               | radius(8.F);
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
         | tone_of(tone::accent) | radius(4.F);
}
