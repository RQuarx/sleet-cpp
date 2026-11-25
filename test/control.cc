#include <string>
#include <string_view>

#include <boost/ut.hpp>

#include "sleet/widget/control.hh"


const boost::ut::suite control_tests = []
{
    using namespace boost::ut;
    using namespace sleet::widget;

    "offset helpers walk code points"_test = []
    {
        const auto text = std::string_view { "a\xC3\xA9\xF0\x9F\x91\x8D"
                                             "b" };

        expect(eq(next_offset(text, 0), 1_u));
        expect(eq(next_offset(text, 1), 3_u));
        expect(eq(next_offset(text, 3), 7_u));
        expect(eq(next_offset(text, 7), 8_u));
        expect(eq(next_offset(text, 8), 8_u));

        expect(eq(previous_offset(text, 8), 7_u));
        expect(eq(previous_offset(text, 7), 3_u));
        expect(eq(previous_offset(text, 3), 1_u));
        expect(eq(previous_offset(text, 1), 0_u));
        expect(eq(previous_offset(text, 0), 0_u));
    };

    "clamp_offset snaps to a code point boundary"_test = []
    {
        const auto text = std::string_view { "a\xC3\xA9z" };

        expect(eq(clamp_offset(text, 0), 0_u));
        expect(eq(clamp_offset(text, 1), 1_u));
        expect(eq(clamp_offset(text, 2), 1_u));
        expect(eq(clamp_offset(text, 3), 3_u));
        expect(eq(clamp_offset(text, 99), 4_u));
    };

    "insert replaces the selected range"_test = []
    {
        auto text = std::string { "hello world" };
        auto edit = text_selection { .caret = 11, .anchor = 6 };

        insert_text(text, edit, "there");

        expect(eq(text, std::string { "hello there" }));
        expect(eq(edit.caret, 11_u));
        expect(eq(edit.anchor, 11_u));
    };

    "backspace and delete respect the selection"_test = []
    {
        auto text = std::string { "abcdef" };
        auto edit = text_selection { .caret = 4, .anchor = 2 };

        expect(erase_backward(text, edit));
        expect(eq(text, std::string { "abef" }));
        expect(eq(edit.caret, 2_u));

        edit.collapse(2);
        expect(erase_forward(text, edit));
        expect(eq(text, std::string { "abf" }));
        expect(eq(edit.caret, 2_u));

        edit.collapse(0);
        expect(!erase_backward(text, edit));
    };

    "select all and selected_text copy the range"_test = []
    {
        const auto text = std::string_view { "sleet" };
        auto       edit = text_selection {};

        select_all(text, edit);
        expect(edit.has_selection());
        expect(eq(edit.begin(), 0_u));
        expect(eq(edit.end(), 5_u));
        expect(eq(selected_text(text, edit), std::string { "sleet" }));
    };

    "move_caret keeps the anchor when extending"_test = []
    {
        const auto text = std::string_view { "abcdef" };
        auto       edit = text_selection {};

        move_caret(text, edit, 4, false);
        expect(eq(edit.caret, 4_u));
        expect(eq(edit.anchor, 4_u));

        move_caret(text, edit, 2, true);
        expect(eq(edit.caret, 2_u));
        expect(eq(edit.anchor, 4_u));
    };

    "sanitize_single_line replaces control characters with spaces"_test = []
    {
        expect(eq(sanitize_single_line("a\r\nb\tc"), std::string { "a b c" }));
        expect(eq(sanitize_single_line("a\nb"), std::string { "a b" }));
        expect(eq(sanitize_single_line("a\rb"), std::string { "a b" }));
        expect(eq(sanitize_single_line(std::string { "a\0b", 3 }), std::string { "a b" }));
        expect(eq(sanitize_single_line("plain"), std::string { "plain" }));

        const auto invalid = std::string { "a\xFF" };
        expect(sanitize_single_line(invalid).empty());
    };
};


auto main() -> int {}
