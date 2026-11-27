#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <boost/ut.hpp>

#include "sleet/text/wrap.hh"


namespace
{
    auto measure(std::string_view line) -> kei::result<float>
    { return static_cast<float>(line.size()) * 10.F; }


    auto lines_of(std::string_view text, std::optional<float> width, bool break_anywhere = false)
        -> std::vector<std::string_view>
    {
        auto wrapped = sleet::txt::wrap(text, width, measure, break_anywhere);
        if (!wrapped) throw std::runtime_error { wrapped.error().what() };

        std::vector<std::string_view> result;
        result.reserve(wrapped->size());
        for (const auto &line : *wrapped) result.push_back(line.text);
        return result;
    }
}


const boost::ut::suite wrap_tests = []
{
    using namespace boost::ut;
    using namespace std::string_view_literals;

    "text wraps at the last opportunity that fits"_test = []
    {
        const auto lines = lines_of("hello world", 60.F);
        expect(eq(lines.size(), 2_u));
        expect(eq(lines[0], "hello"sv));
        expect(eq(lines[1], "world"sv));
    };

    "unbreakable words overflow instead of disappearing"_test = []
    {
        const auto lines = lines_of("abcdefghij", 30.F);
        expect(eq(lines.size(), 1_u));
        expect(eq(lines[0], "abcdefghij"sv));
    };

    "mandatory breaks are always honoured"_test = []
    {
        const auto lines = lines_of("one\ntwo\nthree", std::nullopt);
        expect(eq(lines.size(), 3_u));
        expect(eq(lines[0], "one"sv));
        expect(eq(lines[1], "two"sv));
        expect(eq(lines[2], "three"sv));
    };

    "without a limit the text stays on one line"_test = []
    {
        const auto lines = lines_of("hello world", std::nullopt);
        expect(eq(lines.size(), 1_u));
        expect(eq(lines[0], "hello world"sv));
    };

    "trailing whitespace is trimmed"_test = []
    {
        const auto lines = lines_of("hello   \nworld  ", std::nullopt);
        expect(eq(lines.size(), 2_u));
        expect(eq(lines[0], "hello"sv));
        expect(eq(lines[1], "world"sv));
    };

    "a zero width limit breaks at every opportunity"_test = []
    {
        const auto lines = lines_of("hello world", 0.F);
        expect(eq(lines.size(), 2_u));
        expect(eq(lines[0], "hello"sv));
        expect(eq(lines[1], "world"sv));
    };

    "a negative width limit is clamped to zero"_test = []
    {
        const auto lines = lines_of("hello world", -10.F);
        expect(eq(lines.size(), 2_u));
        expect(eq(lines[0], "hello"sv));
        expect(eq(lines[1], "world"sv));
    };

    "empty text yields no lines"_test = [] { expect(lines_of("", 10.F).empty()); };

    "wrap anywhere splits a word that does not fit"_test = []
    {
        const auto lines = lines_of("abcdefghij", 30.F, true);
        expect(eq(lines.size(), 4_u));
        expect(eq(lines[0], "abc"sv));
        expect(eq(lines[1], "def"sv));
        expect(eq(lines[2], "ghi"sv));
        expect(eq(lines[3], "j"sv));
    };

    "wrap anywhere keeps grapheme clusters together"_test = []
    {
        // "e" followed by a combining acute accent is one grapheme cluster.
        const auto lines = lines_of("ae\xCC\x81"
                                    "b"sv,
                                    20.F, true);
        expect(eq(lines.size(), 3_u));
        expect(eq(lines[0], "a"sv));
        expect(eq(lines[1], "e\xCC\x81"sv));
        expect(eq(lines[2], "b"sv));
    };

    "wrap anywhere does not emit blank lines for skipped spaces"_test = []
    {
        const auto lines = lines_of("abc def", 10.F, true);
        expect(eq(lines.size(), 6_u));
        expect(eq(lines[0], "a"sv));
        expect(eq(lines[1], "b"sv));
        expect(eq(lines[2], "c"sv));
        expect(eq(lines[3], "d"sv));
        expect(eq(lines[4], "e"sv));
        expect(eq(lines[5], "f"sv));
    };

    "wrap anywhere still prefers a break opportunity"_test = []
    {
        const auto lines = lines_of("aa bbbbbbbbbb", 30.F, true);
        expect(eq(lines.size(), 5_u));
        expect(eq(lines[0], "aa"sv));
        expect(eq(lines[1], "bbb"sv));
        expect(eq(lines[2], "bbb"sv));
        expect(eq(lines[3], "bbb"sv));
        expect(eq(lines[4], "b"sv));
    };

    "hard breaks mark paragraph ends and the end of the text"_test = []
    {
        auto wrapped = sleet::txt::wrap("aa bb\ncc"sv, 20.F, measure);
        expect(static_cast<bool>(wrapped));
        if (!wrapped) return;

        expect(eq(wrapped->size(), 3_u));
        expect(!(*wrapped)[0].hard_break);
        expect((*wrapped)[1].hard_break);
        expect((*wrapped)[2].hard_break);
    };

    "wrapped lines carry byte offsets into the source"_test = []
    {
        auto wrapped = sleet::txt::wrap("aa bb\ncc"sv, 20.F, measure);
        expect(static_cast<bool>(wrapped));
        if (!wrapped) return;

        // The space at the soft break is skipped, so the second line starts
        // at "bb". The hard break skips the newline before "cc".
        expect(eq(wrapped->size(), 3_u));
        expect(eq((*wrapped)[0].begin, 0_u));
        expect(eq((*wrapped)[1].begin, 3_u));
        expect(eq((*wrapped)[2].begin, 6_u));
    };

    "invalid utf-8 is reported"_test = []
    {
        const auto result = sleet::txt::wrap("a\xE6\x97"sv, 10.F, measure);
        expect(!result);
    };
};


auto main() -> int {}
