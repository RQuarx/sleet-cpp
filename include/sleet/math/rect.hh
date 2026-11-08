#pragma once
#include <format>

#include "sleet/math/vec2.hh"
#include "sleet/math/vec4.hh"


namespace sleet::math
{
    struct rect
    {
        vec2 position;
        vec2 size;


        constexpr rect(vec2 pos, vec2 size) noexcept : position { pos }, size { size } {}
        constexpr rect(vec4 vec) noexcept : position { vec.x, vec.y }, size { vec.w, vec.h } {}
        constexpr rect(float x, float y, float w, float h) noexcept
            : position { x, y }, size { w, h }
        {
        }

        template <typename T>
        [[nodiscard]]
        constexpr auto to() const noexcept -> T
        { return T { position.x, position.y, size.w, size.h }; }
    };
}


template <>
struct std::formatter<sleet::math::rect>
{
    template <class ParseContext>
    constexpr auto parse(ParseContext &ctx) const noexcept -> ParseContext::iterator
    { return ctx.begin(); }

    template <class FmtContext>
    auto format(const sleet::math::rect s, FmtContext &ctx) const -> FmtContext::iterator
    {
        return std::format_to(ctx.out(), "[({}, {}), ({}, {})]", s.position.x, s.position.y,
                              s.size.w, s.size.h);
    }
};
