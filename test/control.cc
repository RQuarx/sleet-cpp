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

    "offset helpers keep grapheme clusters together"_test = []
    {
        // e + combining acute accent.
        const auto combined = std::string_view { "e\xCC\x81x" };
        expect(eq(next_offset(combined, 0), 3_u));
        expect(eq(next_offset(combined, 3), 4_u));
        expect(eq(previous_offset(combined, 4), 3_u));
        expect(eq(previous_offset(combined, 3), 0_u));

        // Woman technologist: a ZWJ sequence.
        const auto joined = std::string_view { "\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x92\xBB" };
        expect(eq(next_offset(joined, 0), joined.size()));
        expect(eq(previous_offset(joined, joined.size()), 0_u));

        // A trailing joiner still belongs to the cluster before it.
        const auto trailing_joiner = std::string_view { "a\xE2\x80\x8D" };
        expect(eq(next_offset(trailing_joiner, 0), trailing_joiner.size()));
        expect(eq(previous_offset(trailing_joiner, trailing_joiner.size()), 0_u));

        // A regional-indicator flag pair.
        const auto flag = std::string_view { "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8" };
        expect(eq(next_offset(flag, 0), flag.size()));
        expect(eq(previous_offset(flag, flag.size()), 0_u));

        // An odd indicator run pairs from the start, so the final indicator is
        // its own cluster instead of being joined to the previous one.
        const auto flags = std::string_view { "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8\xF0\x9F\x87\xBA" };
        expect(eq(next_offset(flags, 0), 8_u));
        expect(eq(next_offset(flags, 8), flags.size()));
        expect(eq(previous_offset(flags, flags.size()), 8_u));
        expect(eq(previous_offset(flags, 8), 0_u));
    };

    "word helpers stop at word and punctuation boundaries"_test = []
    {
        const auto text = std::string_view { "hello, world" };

        expect(eq(next_word_offset(text, 0), 5_u));
        expect(eq(next_word_offset(text, 5), 7_u));
        expect(eq(next_word_offset(text, 7), 12_u));

        expect(eq(previous_word_offset(text, 12), 7_u));
        expect(eq(previous_word_offset(text, 7), 0_u));

        const auto [first, last] = word_range_at(text, 9);
        expect(eq(first, 7_u));
        expect(eq(last, 12_u));
    };

    "word deletion removes the whole word"_test = []
    {
        auto text = std::string { "hello, world" };
        auto edit = text_selection { .caret = 12, .anchor = 12 };

        expect(erase_word_backward(text, edit));
        expect(eq(text, std::string { "hello, " }));
        expect(eq(edit.caret, 7_u));

        edit.collapse(0);
        expect(erase_word_forward(text, edit));
        expect(eq(text, std::string { ", " }));
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

    "sanitize_multiline normalizes line endings and keeps newlines"_test = []
    {
        expect(eq(sanitize_multiline("a\r\nb\tc"), std::string { "a\nb c" }));
        expect(eq(sanitize_multiline("a\nb"), std::string { "a\nb" }));
        expect(eq(sanitize_multiline("a\rb"), std::string { "a\nb" }));
        expect(eq(sanitize_multiline(std::string { "a\0b", 3 }), std::string { "a b" }));
        expect(eq(sanitize_multiline("plain"), std::string { "plain" }));

        const auto invalid = std::string { "a\xFF" };
        expect(sanitize_multiline(invalid).empty());
    };
};


auto main() -> int {}
