#pragma once
#include <algorithm>
#include <format>

#include "sleet/math/edge_insets.hh"
#include "sleet/math/vec2.hh"


namespace sleet::math
{
    struct rect
    {
        vec2 position;
        vec2 size;


        constexpr rect() noexcept = default;
        constexpr rect(vec2 pos, vec2 extent) noexcept : position { pos }, size { extent } {}
        constexpr rect(float x, float y, float width, float height) noexcept
            : position { x, y }, size { width, height }
        {
        }


        [[nodiscard]]
        constexpr auto x() const noexcept -> float
        { return position.x; }

        [[nodiscard]]
        constexpr auto y() const noexcept -> float
        { return position.y; }

        [[nodiscard]]
        constexpr auto width() const noexcept -> float
        { return size.x; }

        [[nodiscard]]
        constexpr auto height() const noexcept -> float
        { return size.y; }

        [[nodiscard]]
        constexpr auto right() const noexcept -> float
        { return position.x + size.x; }

        [[nodiscard]]
        constexpr auto bottom() const noexcept -> float
        { return position.y + size.y; }

        [[nodiscard]]
        constexpr auto center() const noexcept -> vec2
        { return { position.x + (size.x / 2.F), position.y + (size.y / 2.F) }; }

        [[nodiscard]]
        constexpr auto contains(vec2 point) const noexcept -> bool
        {
            return point.x >= position.x and point.y >= position.y and point.x < right()
               and point.y < bottom();
        }

        [[nodiscard]]
        constexpr auto inset(edge_insets edges) const noexcept -> rect
        {
            return {
                { position.x + edges.left, position.y + edges.top },
                { std::max(0.F, size.x - edges.horizontal()),
                 std::max(0.F, size.y - edges.vertical()) }
            };
        }

        [[nodiscard]]
        friend constexpr auto operator==(rect, rect) noexcept -> bool = default;
    };
}


template <>
struct std::formatter<sleet::math::rect>
{
    template <class ParseContext>
    constexpr auto parse(ParseContext &ctx) const noexcept -> ParseContext::iterator
    { return ctx.begin(); }

    template <class FmtContext>
    auto format(const sleet::math::rect &r, FmtContext &ctx) const -> FmtContext::iterator
    {
        return std::format_to(ctx.out(), "[({}, {}), ({} x {})]", r.position.x, r.position.y,
                              r.size.x, r.size.y);
    }
};
