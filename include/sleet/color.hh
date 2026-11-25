#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>

#include <SDL3/SDL_pixels.h>


namespace sleet
{
    struct color final
    {
        std::uint8_t r = 0;
        std::uint8_t g = 0;
        std::uint8_t b = 0;
        std::uint8_t a = 0;


        [[nodiscard]]
        static constexpr auto from_rgb(std::uint32_t rgb, std::uint8_t alpha = 255) noexcept
            -> color
        {
            return {
                static_cast<std::uint8_t>((rgb >> 16U) & 0xFFU),
                static_cast<std::uint8_t>((rgb >> 8U) & 0xFFU),
                static_cast<std::uint8_t>(rgb & 0xFFU),
                alpha,
            };
        }

        [[nodiscard]]
        static constexpr auto from_rgba(std::uint32_t rgba) noexcept -> color
        {
            return {
                static_cast<std::uint8_t>((rgba >> 24U) & 0xFFU),
                static_cast<std::uint8_t>((rgba >> 16U) & 0xFFU),
                static_cast<std::uint8_t>((rgba >> 8U) & 0xFFU),
                static_cast<std::uint8_t>(rgba & 0xFFU),
            };
        }


        [[nodiscard]]
        constexpr auto to_sdl() const noexcept -> SDL_Color
        { return { r, g, b, a }; }

        [[nodiscard]]
        constexpr auto to_sdl_float() const noexcept -> SDL_FColor
        {
            return {
                .r = static_cast<float>(r) / 255.F,
                .g = static_cast<float>(g) / 255.F,
                .b = static_cast<float>(b) / 255.F,
                .a = static_cast<float>(a) / 255.F,
            };
        }


        [[nodiscard]]
        auto lerp(color target, float delta) const noexcept -> color
        {
            const auto amount = std::isfinite(delta) ? std::clamp(delta, 0.F, 1.F) : 0.F;

            const auto mix = [amount](std::uint8_t from, std::uint8_t to) -> std::uint8_t
            {
                return static_cast<std::uint8_t>(std::lround(
                    std::lerp(static_cast<float>(from), static_cast<float>(to), amount)));
            };

            return { mix(r, target.r), mix(g, target.g), mix(b, target.b), mix(a, target.a) };
        }


        [[nodiscard]]
        constexpr auto is_transparent() const noexcept -> bool
        { return a == 0; }

        [[nodiscard]]
        constexpr auto is_opaque() const noexcept -> bool
        { return a == 255; }

        [[nodiscard]]
        friend constexpr auto operator==(color lhs, color rhs) noexcept -> bool = default;
    };
}


namespace sleet::literals
{
    [[nodiscard]]
    constexpr auto operator""_rgb(unsigned long long hex) noexcept -> sleet::color
    { return sleet::color::from_rgb(static_cast<std::uint32_t>(hex)); }

    [[nodiscard]]
    constexpr auto operator""_rgba(unsigned long long hex) noexcept -> sleet::color
    { return sleet::color::from_rgba(static_cast<std::uint32_t>(hex)); }
}


template <>
struct std::formatter<sleet::color>
{
    template <class ParseContext>
    constexpr auto parse(ParseContext &ctx) const noexcept -> ParseContext::iterator
    { return ctx.begin(); }

    template <class FmtContext>
    auto format(const sleet::color &c, FmtContext &ctx) const -> FmtContext::iterator
    { return std::format_to(ctx.out(), "#{:02x}{:02x}{:02x}{:02x}", c.r, c.g, c.b, c.a); }
};
