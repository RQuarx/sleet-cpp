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


    auto lines_of(std::string_view text, std::optional<float> width)
        -> std::vector<std::string_view>
    {
        auto wrapped = sleet::txt::wrap(text, width, measure);
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

    "invalid utf-8 is reported"_test = []
    {
        const auto result = sleet::txt::wrap("a\xE6\x97"sv, 10.F, measure);
        expect(!result);
    };
};


auto main() -> int {}
