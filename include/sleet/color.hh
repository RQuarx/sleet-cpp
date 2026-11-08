#pragma once
#include <cmath>
#include <cstdint>
#include <initializer_list>

#include <SDL3/SDL_pixels.h>


namespace sleet
{
    struct color final
    {
        std::uint8_t r = 0;
        std::uint8_t g = 0;
        std::uint8_t b = 0;
        std::uint8_t a = 0;


        operator SDL_Color() const noexcept { return *reinterpret_cast<const SDL_Color *>(this); }
        operator SDL_FColor() const noexcept
        {
            return {
                .r = static_cast<float>(r) / 255.F,
                .g = static_cast<float>(g) / 255.F,
                .b = static_cast<float>(b) / 255.F,
                .a = static_cast<float>(a) / 255.F,
            };
        }


        [[nodiscard]]
        auto lerp(color final, float delta) const noexcept -> color
        {
            color next = *this;
            for (std::uint8_t color::*member : { &color::r, &color::g, &color::b, &color::a })
                std::lerp(next.*member, final.*member, delta);
            return next;
        }


        [[nodiscard]]
        auto is_unset() const noexcept -> bool
        { return r == 0 and g == 0 and b == 0 and a == 0; }


        [[nodiscard]]
        friend auto operator==(color lhs, color rhs) noexcept -> bool = default;
    };
}


[[nodiscard]]
auto operator""_rgba(unsigned long long hex) noexcept -> sleet::color
{
    sleet::color color;

    for (std::uint8_t i = 0; i < 4; i++)
        reinterpret_cast<std::uint8_t *>(&color)[i] = (hex >> (i * 8)) & 255;

    return color;
}


[[nodiscard]]
auto operator""_rgb(unsigned long long hex) noexcept -> sleet::color
{
    sleet::color color;

    for (std::uint8_t i = 0; i < 3; i++)
        reinterpret_cast<std::uint8_t *>(&color)[i] = (hex >> (i * 8)) & 255;

    return color;
}
