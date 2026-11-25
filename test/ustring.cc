#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <boost/ut.hpp>

#include "sleet/text/ustring.hh"


namespace
{
    using sleet::txt::ustring;
    using sleet::txt::ustring_view;
    using namespace std::string_view_literals;


    auto make(std::string_view text) -> ustring
    {
        auto result = ustring::from_utf8(text);
        if (!result) throw std::runtime_error { result.error().what() };
        return std::move(*result);
    }
}


const boost::ut::suite ustring_tests = []
{
    using namespace boost::ut;
    using namespace std::string_literals;

    "construction validates utf-8"_test = []
    {
        expect(static_cast<bool>(ustring::from_utf8("hello"sv)));
        expect(static_cast<bool>(ustring::from_utf8("h\xC3\xA9llo"sv)));
        expect(static_cast<bool>(ustring::from_utf8("\xF0\x9F\x98\x80"sv)));

        expect(!ustring::from_utf8("abc\xE6\x97"sv));
        expect(!ustring::from_utf8("a\x97"
                                   "b"sv));
    };

    "length counts code points while size counts bytes"_test = []
    {
        const auto text = make("h\xC3\xA9llo \xF0\x9F\x98\x80"sv);
        expect(eq(text.length(), 7_u));
        expect(eq(text.size(), 11_u));
        expect(text.front() == U'h');
        expect(text.back() == U'\U0001F600');
    };

    "append and insert preserve whole code points"_test = []
    {
        auto text = make("ab"sv);
        expect(static_cast<bool>(text.push_back(U'\u00E9')));
        expect(eq(text.string(), "ab\xC3\xA9"s));

        expect(static_cast<bool>(text.insert(1, U'\U0001F600')));
        expect(eq(text.string(), "a\xF0\x9F\x98\x80"
                                 "b\xC3\xA9"s));

        expect(static_cast<bool>(text.insert(0, "x"sv)));
        expect(eq(text.string(), "xa\xF0\x9F\x98\x80"
                                 "b\xC3\xA9"s));
    };

    "append and insert accept views into the same string"_test = []
    {
        auto text = make("a string long enough to reallocate"sv);
        text.shrink_to_fit();
        const auto whole = text.view();
        expect(static_cast<bool>(text.append(whole)));
        expect(eq(text.string(),
                  "a string long enough to reallocatea string long enough to reallocate"s));

        text              = make("abc"sv);
        const auto middle = text.view().substr(1);
        expect(static_cast<bool>(text.insert(1, middle)));
        expect(eq(text.string(), "abcbc"s));

        text = make("reallocate"sv);
        text.shrink_to_fit();
        expect(static_cast<bool>(text.append(text.c_str(), text.size())));
        expect(eq(text.string(), "reallocatereallocate"s));
    };

    "erase removes whole code points"_test = []
    {
        auto text = make("a\xC3\xA9\xF0\x9F\x98\x80z"sv);
        expect(static_cast<bool>(text.erase(1, 2)));
        expect(eq(text.string(), "az"s));
        expect(!text.erase(2, 1));
        expect(!text.erase(0, 3));
    };

    "pop_back removes the last code point"_test = []
    {
        auto text = make("a\xF0\x9F\x98\x80"sv);
        expect(static_cast<bool>(text.pop_back()));
        expect(eq(text.string(), "a"s));

        auto empty = ustring {};
        expect(!empty.pop_back());
    };

    "resize grows with spaces and shrinks by code point"_test = []
    {
        auto text = make("h\xC3\xA9llo"sv);
        expect(static_cast<bool>(text.resize(3)));
        expect(eq(text.string(), "h\xC3\xA9l"s));

        expect(static_cast<bool>(text.resize(5)));
        expect(eq(text.string(), "h\xC3\xA9l  "s));
        expect(eq(text.length(), 5_u));
    };

    "invalid code points are rejected"_test = []
    {
        auto text = ustring {};
        expect(!text.push_back(0xD800));
        expect(!text.push_back(0x110000));
        expect(static_cast<bool>(text.push_back(0x10FFFF)));
    };

    "index errors are reported"_test = []
    {
        auto text = make("abc"sv);
        expect(!text.insert(4, U'x'));
        expect(!text.insert(4, "x"sv));
    };

    "iterators decode code points"_test = []
    {
        const auto text = make("a\xC3\xA9\xF0\x9F\x98\x80"sv);

        std::u32string decoded;
        for (const auto cp : text) decoded.push_back(cp);

        expect(decoded == U"a\x00E9\U0001F600");
    };

    "views share the bytes and validate"_test = []
    {
        const auto text = make("h\xC3\xA9llo"sv);
        auto       view = ustring_view::from_utf8(text.view());
        expect(static_cast<bool>(view));
        expect(eq(view->length(), 5_u));
        expect(eq(view->size(), 6_u));
        expect(view->front() == U'h');
        expect(view->back() == U'o');

        expect(!ustring_view::from_utf8("a\x97"
                                        "b"sv));
    };
};


auto main() -> int {}
