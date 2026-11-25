#pragma once
#include <cstddef>
#include <cstdint>
#include <format>
#include <string_view>
#include <vector>

#include <kei-utils/result.hh>


namespace sleet::txt
{
    enum class break_kind : std::uint8_t
    {
        mandatory = 0,
        allowed   = 1,
    };


    struct break_opportunity
    {
        std::size_t offset = 0; /* byte offset */
        break_kind  kind   = break_kind::allowed;


        [[nodiscard]] static auto of(std::vector<break_opportunity> &opportunities,
                                     std::string_view                text,
                                     const char *lang = nullptr) -> kei::result<>;

        [[nodiscard]] static auto of(std::string_view text, const char *lang = nullptr)
            -> kei::result<std::vector<break_opportunity>>;
    };
}


template <>
struct std::formatter<sleet::txt::break_opportunity>
{
    template <class ParseContext>
    constexpr auto parse(ParseContext &ctx) const noexcept -> ParseContext::iterator
    { return ctx.begin(); }

    template <class FmtContext>
    auto format(const sleet::txt::break_opportunity &opportunity, FmtContext &ctx) const
        -> FmtContext::iterator
    {
        return std::format_to(ctx.out(), "{}:{}", opportunity.offset,
                              opportunity.kind == sleet::txt::break_kind::mandatory ? 'M' : 'A');
    }
};
