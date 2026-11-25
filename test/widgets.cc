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
    };

    "combo boxes contain a header and hidden options"_test = []
    {
        auto        widget = combo_box("Green", { "Red", "Green", "Blue" }, "color", "Pick one");
        const auto &data   = widget.data_view();

        expect(data.is_column());
        expect(eq(data.children.size(), 4_u));

        const auto &header = data.children[0];
        expect(header.control == control_kind::combo_box);
        expect(header.focusable);
        expect(eq(header.action, std::string { "color" }));

        for (std::size_t i = 1; i < data.children.size(); i++)
        {
            const auto &option = data.children[i];
            expect(option.control == control_kind::combo_option);
            expect(!option.visible);
            expect(eq(option.action, std::string { "color" }));
            expect(!option.text.empty());
        }

        expect(eq(data.children[1].text, std::string { "Red" }));
        expect(eq(data.children[3].text, std::string { "Blue" }));
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

        expect(high.control == control_kind::slider);
        expect(high.focusable);
        expect(eq(high.control_value, 1.F));
        expect(eq(low.control_value, 0.F));

        const auto &track = high.children[0];
        expect(track.is_row());
        expect(eq(track.children.size(), 3_u));
        expect(eq(track.children[0].flex, 1.F));
        expect(eq(track.children[2].flex, 0.F));
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
};


auto main() -> int {}
