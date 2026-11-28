#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <boost/ut.hpp>

#include "sleet/widget/modifier.hh"
#include "sleet/widgets.hh"


const boost::ut::suite widget_tests = []
{
    using namespace boost::ut;
    using namespace sleet::widget;

    "buttons expose an action and a variant"_test = []
    {
        auto        widget = button("Save", "save", button_variant::primary);
        const auto &data   = widget.data_view();

        expect(data.is_box());
        expect(data.focusable);
        expect(eq(data.action, std::string { "save" }));
        expect(data.variant == button_variant::primary);
        expect(eq(data.children.size(), 1_u));
        expect(data.children[0].is_text());
    };

    "buttons accept arbitrary content"_test = []
    {
        auto        widget = button(to_data(row()[label("A"), label("B")]), "go");
        const auto &data   = widget.data_view();

        expect(eq(data.action, std::string { "go" }));
        expect(eq(data.children.size(), 1_u));
        expect(data.children[0].is_row());
    };

    "text inputs carry editing metadata"_test = []
    {
        auto        widget = text_input("hello", "name", "Type here...");
        const auto &data   = widget.data_view();

        expect(data.is_text());
        expect(data.control == control_kind::text_input);
        expect(data.focusable);
        expect(eq(data.text, std::string { "hello" }));
        expect(eq(data.placeholder, std::string { "Type here..." }));
        expect(eq(data.action, std::string { "name" }));
        expect(data.size.x.is_fill());
        expect(eq(data.size.y.get_fixed_size(), 36.F));
        expect(!data.multiline);
    };

    "text areas enable multiline editing with a taller default"_test = []
    {
        auto        widget = text_area("hello\nworld", "notes", "Write...");
        const auto &data   = widget.data_view();

        expect(data.is_text());
        expect(data.control == control_kind::text_input);
        expect(data.focusable);
        expect(data.multiline);
        expect(eq(data.text, std::string { "hello\nworld" }));
        expect(eq(data.placeholder, std::string { "Write..." }));
        expect(eq(data.action, std::string { "notes" }));
        expect(data.size.x.is_fill());
        expect(eq(data.size.y.get_fixed_size(), 120.F));
        expect(data.wrap == text_wrap_mode::wrap);
    };

    "floating boxes anchor to parent edges and configure stacking"_test = []
    {
        const auto data = floating_box(anchor_point::bottom_right, 12).data_view();

        expect(data.is_box());
        expect(data.floating);
        expect(data.placement_anchor == anchor_point::bottom_right);
        expect(eq(data.z_index, 12));

        auto        adjusted      = floating_box() | anchor_at(anchor_point::center) | stacking(7);
        const auto &adjusted_data = adjusted.data_view();
        expect(adjusted_data.placement_anchor == anchor_point::center);
        expect(eq(adjusted_data.z_index, 7));
    };

    "combo boxes contain a header and a floating popup"_test = []
    {
        auto        widget = combo_box("Green", { "Red", "Green", "Blue" }, "color", "Pick one");
        const auto &data   = widget.data_view();

        expect(data.is_column());
        expect(eq(data.children.size(), 2_u));

        const auto &header = data.children[0];
        expect(header.control == control_kind::combo_box);
        expect(header.focusable);
        expect(eq(header.action, std::string { "color" }));

        const auto &popup = data.children[1];
        expect(popup.is_box());
        expect(popup.floating);
        expect(popup.control == control_kind::combo_popup);
        expect(popup.placement_anchor == anchor_point::top_left);
        expect(eq(popup.placement.y, 38.F));
        expect(eq(popup.z_index, 100));
        expect(popup.size.x.is_fill());
        expect(!popup.visible);
        expect(eq(popup.children.size(), 1_u));

        const auto &options = popup.children[0];
        expect(options.is_column());
        expect(eq(options.children.size(), 3_u));

        for (const auto &option : options.children)
        {
            expect(option.control == control_kind::combo_option);
            expect(option.visible);
            expect(eq(option.action, std::string { "color" }));
            expect(!option.text.empty());
        }

        expect(eq(options.children[0].text, std::string { "Red" }));
        expect(eq(options.children[2].text, std::string { "Blue" }));
    };

    "checkboxes and radios mark their indicators"_test = []
    {
        auto check = checkbox("Remember", true, "remember");
        auto radio = radio_button("One", false, "one");

        const auto &check_data = check.data_view();
        expect(check_data.focusable);
        expect(eq(check_data.action, std::string { "remember" }));

        const auto &indicator = check_data.children[0].children[0];
        expect(indicator.control == control_kind::checkbox_indicator);
        expect(indicator.checked);

        const auto &radio_indicator = radio.data_view().children[0].children[0];
        expect(radio_indicator.control == control_kind::radio_indicator);
        expect(!radio_indicator.checked);
    };

    "toggles align the thumb with the state"_test = []
    {
        const auto on  = toggle("On", true, "on").data_view();
        const auto off = toggle("Off", false, "off").data_view();

        const auto &on_track  = on.children[0].children[0];
        const auto &off_track = off.children[0].children[0];

        expect(on_track.alignment.x == align::end);
        expect(off_track.alignment.x == align::start);
        expect(on_track.tint == tone::accent);
        expect(off_track.tint == tone::field);
    };

    "sliders clamp and distribute their value"_test = []
    {
        const auto high = slider(2.F, "volume").data_view();
        const auto low  = slider(-1.F, "volume").data_view();
        const auto nan  = slider(std::numeric_limits<float>::quiet_NaN(), "volume").data_view();

        expect(high.control == control_kind::slider);
        expect(high.focusable);
        expect(eq(high.control_value, 1.F));
        expect(eq(low.control_value, 0.F));

        // NaN would otherwise survive std::clamp and poison the thumb position.
        expect(eq(nan.control_value, 0.F));
        expect(high.children.empty());

        // The renderer draws the track, fill, and thumb directly from the
        // control value, so the slider is one node instead of three segments.
        expect(high.size.x.is_fill());
        expect(eq(high.size.y.get_fixed_size(), 20.F));
    };

    "progress bars split the track by value"_test = []
    {
        const auto bar = progress_bar(0.25F).data_view();

        expect(bar.is_row());
        expect(eq(bar.children.size(), 2_u));
        expect(eq(bar.children[0].flex, 0.25F));
        expect(eq(bar.children[1].flex, 0.75F));
        expect(eq(bar.size.y.get_fixed_size(), 8.F));
    };

    "separators, spacers, and badges use theme tones"_test = []
    {
        const auto rule  = separator().data_view();
        const auto space = spacer().data_view();
        const auto tag   = badge("new").data_view();

        expect(eq(rule.size.y.get_fixed_size(), 1.F));
        expect(rule.tint == tone::border);

        expect(space.size.x.is_fill());
        expect(space.size.y.is_fill());

        expect(tag.is_text());
        expect(tag.tint == tone::accent);
        expect(eq(tag.text, std::string { "new" }));
    };

    "list views group focusable rows"_test = []
    {
        const auto view = list_view().data_view();

        expect(view.control == control_kind::list_view);
        expect(view.focus_group);
        expect(view.scrollable);
        expect(view.clip_children);
        expect(view.size.x.is_fill());
        expect(eq(view.size.y.get_fixed_size(), 240.F));

        const auto row = list_item("Alpha", "alpha", true).data_view();
        expect(row.control == control_kind::list_item);
        expect(row.focusable);
        expect(row.selected);
        expect(eq(row.action, std::string { "alpha" }));
        expect(eq(row.text, std::string { "Alpha" }));
        expect(eq(row.children.size(), 1_u));
        expect(row.children[0].is_text());
        expect(eq(row.size.y.get_fixed_size(), 32.F));
    };

    "table rows and cells share weighted columns"_test = []
    {
        const auto view = table_view().data_view();
        expect(view.control == control_kind::list_view);
        expect(view.focus_group);
        expect(view.scrollable);

        auto header = table_header();
        header = std::move(header)[table_header_cell("Name", 2.F), table_header_cell("Size", 1.F)];
        const auto &header_data = header.data_view();
        expect(eq(header_data.children.size(), 2_u));
        expect(eq(header_data.children[0].flex, 2.F));
        expect(eq(header_data.children[1].flex, 1.F));
        expect(header_data.children[0].children[0].font.weight == font_weight::semibold);

        const auto row = table_row("open", true).data_view();
        expect(row.is_row());
        expect(row.control == control_kind::list_item);
        expect(row.selected);
        expect(row.focusable);

        const auto cell = table_cell("Alpha", 2.F).data_view();
        expect(cell.is_box());
        expect(cell.size.x.is_grow());
        expect(cell.size.y.is_fill());
        expect(eq(cell.flex, 2.F));
        expect(cell.flex_basis.has_value());
        if (cell.flex_basis) expect(eq(*cell.flex_basis, 0.F));
        expect(cell.clip_children);
        expect(cell.children[0].is_text());
        expect(cell.children[0].wrap == text_wrap_mode::no_wrap);

        // Invalid weights cannot collapse a column or poison the flex total.
        expect(eq(table_cell("x", -1.F).data_view().flex, 0.01F));
        expect(eq(table_cell("x", std::numeric_limits<float>::quiet_NaN()).data_view().flex, 1.F));
    };

    "tree rows expose depth and disclosure state"_test = []
    {
        const auto view = tree_view().data_view();
        expect(view.control == control_kind::tree_view);
        expect(view.focus_group);
        expect(view.scrollable);

        const auto row = (tree_item("Docs", "docs",
                                    { .depth         = 2,
                                      .expanded      = true,
                                      .has_children  = true,
                                      .toggle_action = "toggle_docs" })
                          | selected())
                             .data_view();

        expect(row.control == control_kind::tree_item);
        expect(row.focusable);
        expect(row.selected);
        expect(eq(row.tree_depth, 2));
        expect(row.tree_expanded);
        expect(row.tree_has_children);
        expect(eq(row.tree_toggle_action, std::string { "toggle_docs" }));
        expect(eq(row.text, std::string { "Docs" }));
        expect(
            eq(row.padding.left, tree_row_inset + (2.F * tree_indent_width) + tree_chevron_width));
    };

    "depth modifier keeps tree content in its column"_test = []
    {
        const auto row = (tree_item("Child", "child") | depth(2)).data_view();
        expect(eq(row.tree_depth, 2));
        expect(
            eq(row.padding.left, tree_row_inset + (2.F * tree_indent_width) + tree_chevron_width));

        // Repeated and decreasing depths move the inset by the difference
        // only, so modifiers stay idempotent.
        const auto shallower = (tree_item("Child", "child") | depth(2) | depth(1)).data_view();
        expect(eq(shallower.tree_depth, 1));
        expect(eq(shallower.padding.left, tree_row_inset + tree_indent_width + tree_chevron_width));

        const auto deeper = (tree_item("Child", "child") | depth(1) | depth(3)).data_view();
        expect(eq(deeper.tree_depth, 3));
        expect(eq(deeper.padding.left,
                  tree_row_inset + (3.F * tree_indent_width) + tree_chevron_width));
    };
};


auto main() -> int {}
