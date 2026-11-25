#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <boost/ut.hpp>

#include "sleet/text/line_breaking.hh"


namespace
{
    using sleet::txt::break_kind;
    using sleet::txt::break_opportunity;
    using namespace std::string_literals;
    using namespace std::string_view_literals;


    /* Renders opportunities as "<byte offset><A|M>" joined by commas, e.g. "6A,12M".
     * A = allowed, M = mandatory. Failures then print readable expected/actual strings. */
    auto render(const std::vector<break_opportunity> &opps) -> std::string
    {
        std::string out;
        for (const auto &opp : opps)
        {
            if (!out.empty()) out += ',';
            out += std::to_string(opp.offset);
            out += opp.kind == break_kind::mandatory ? 'M' : 'A';
        }
        return out;
    }

    /* Runs of(text, lang) and renders the result, or "<error>" if it failed. */
    auto breaks_of(std::string_view text, const char *lang = nullptr) -> std::string
    {
        auto res = break_opportunity::of(text, lang);
        if (!static_cast<bool>(res)) return "<error>";
        return render(*res);
    }
}


/* Expected values below were checked against libunibreak's actual output for these
 * inputs, and agree with the UAX #14 rules named in the comments. */
const boost::ut::suite line_breaking = []
{
    using namespace boost::ut;

    "ascii: allows a break after a space"_test
        = [] { expect(eq(breaks_of("Hello world"sv), "6A"s)); };

    "ascii: breaks only after the last of several spaces (LB7, LB18)"_test
        = [] { expect(eq(breaks_of("a  b"sv), "3A"s)); };

    "ascii: allows a break after a hyphen"_test
        = [] { expect(eq(breaks_of("well-known"sv), "5A"s)); };

    "ascii: single word has no opportunities"_test = [] { expect(eq(breaks_of("Hello"sv), ""s)); };

    "empty input yields no opportunities and succeeds"_test = []
    {
        auto res = break_opportunity::of(""sv);
        expect(static_cast<bool>(res));
        expect(eq(breaks_of(""sv), ""s));
    };

    "mandatory: break after LF (LB4/LB5)"_test = [] { expect(eq(breaks_of("a\nb"sv), "2M"s)); };

    "mandatory: no break inside CRLF, break after the pair (LB5)"_test
        = [] { expect(eq(breaks_of("a\r\nb"sv), "3M"s)); };

    "mandatory: trailing newline is reported at end of text"_test
        = [] { expect(eq(breaks_of("ab\n"sv), "3M"s)); };

    "multibyte: ideographs may break between characters, never inside one (LB31)"_test = []
    {
        /* U+65E5 U+672C U+8A9E, three bytes each. */
        expect(eq(breaks_of("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"sv), "3A,6A"s));
    };

    "multibyte: no-break space (U+00A0) prevents breaks on both sides (LB12, LB12a)"_test = []
    {
        expect(eq(breaks_of("a\xC2\xA0"
                            "b"sv),
                  ""s));
    };

    "multibyte: valid text is not rejected"_test = []
    {
        /* Regression guard: LINEBREAK_INSIDEACHAR marks the non-final bytes of every
         * multi-byte character, so it must not be treated as an error on its own. */
        expect(static_cast<bool>(break_opportunity::of("caf\xC3\xA9 au lait"sv)));
        expect(static_cast<bool>(break_opportunity::of("\xE6\x97\xA5\xE6\x9C\xAC"sv)));
    };

    "multibyte: every offset lands on a code point boundary"_test = []
    {
        /* "héllo wörld 日本語\n" */
        constexpr auto text = "h\xC3\xA9llo w\xC3\xB6rld \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\n"sv;

        auto res = break_opportunity::of(text);
        expect(static_cast<bool>(res));
        if (!static_cast<bool>(res)) return;

        expect(res->size() > 0_u);
        for (const auto &opp : *res)
        {
            expect(opp.offset > 0_u);
            expect(opp.offset <= text.size());
            if (opp.offset < text.size())
            {
                const auto byte = static_cast<unsigned char>(text[opp.offset]);
                expect((byte & 0xC0U) != 0x80U) << "offset" << opp.offset << "splits a character";
            }
        }
    };

    "language tag is accepted"_test = []
    {
        expect(eq(breaks_of("Hello world"sv, "en"), "6A"s));
        expect(eq(breaks_of("Hello world"sv, "ja"), "6A"s));
    };

    "invalid utf-8: truncated trailing sequence is an error"_test = []
    {
        expect(!break_opportunity::of("abc\xE6\x97"sv));
        expect(!break_opportunity::of("ab\xE6"sv));
    };

    "invalid utf-8: truncated sequence followed by ASCII is an error"_test = []
    {
        /* libunibreak itself does not detect this; it swallows the 'b' as the third byte. */
        expect(!break_opportunity::of("a\xE6\x97"
                                      "b"sv));
    };

    "invalid utf-8: stray continuation byte is an error"_test = []
    {
        expect(!break_opportunity::of("a\x97"
                                      "b"sv));
    };

    "overload taking a vector reports errors"_test = []
    {
        std::vector<break_opportunity> opps;
        expect(!break_opportunity::of(opps, "abc\xE6\x97"sv));
    };
};


auto main() -> int {}
