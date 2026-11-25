#include <linebreak.h>
#include <utf8.h>

#include "sleet/error.hh"
#include "sleet/text/line_breaking.hh"


using sleet::txt::break_kind;
using sleet::txt::break_opportunity;


auto break_opportunity::of(std::vector<break_opportunity> &opportunities,
                           std::string_view                text,
                           const char                     *lang) -> kei::result<>
{
    opportunities.clear();

    if (!utf8::is_valid(text.begin(), text.end()))
        return sleet::error { "Passed string is not valid UTF-8." }.unexpected();

    if (text.empty()) return {};

    std::vector<char> breaks(text.length(), LINEBREAK_NOBREAK);

    set_linebreaks_utf8(reinterpret_cast<const utf8_t *>(text.data()), text.length(), lang,
                        breaks.data());

    for (std::size_t i = 0; i < breaks.size(); i++) switch (breaks[i])
        {
        case LINEBREAK_MUSTBREAK:  opportunities.emplace_back(i + 1, break_kind::mandatory); break;
        case LINEBREAK_ALLOWBREAK: opportunities.emplace_back(i + 1, break_kind::allowed); break;
        default:                   break;
        }

    return {};
}

auto break_opportunity::of(std::string_view text, const char *lang)
    -> kei::result<std::vector<break_opportunity>>
{
    std::vector<break_opportunity> result;
    if (auto res = of(result, text, lang); !res) return std::move(res.error()).unexpected();
    return result;
}
