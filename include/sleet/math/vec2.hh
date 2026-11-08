#pragma once
#include <array>
#include <type_traits>


namespace sleet::math
{
    namespace _impl
    {
        template <typename Tp>
        struct vec2
        {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-anonymous-struct"
#pragma GCC diagnostic ignored "-Wnested-anon-types"
            /* clang-format off */
            union
            {
                std::array<Tp, 2> data;
                struct { Tp x, y; };
                struct { Tp w, h; };
                struct { Tp u, v; };
                struct { Tp a, b; };
                struct { Tp first, second; };
                struct { Tp begin, end; };
                struct { Tp horizontal, vertical; };
            };
            /* clang-format on */
#pragma GCC diagnostic pop


            constexpr vec2(Tp all = Tp {}) noexcept : x { all }, y { all } {}
            constexpr vec2(Tp first, Tp second) noexcept : x { first }, y { second } {}


            [[nodiscard]]
            constexpr auto operator+(vec2 other) noexcept -> vec2
            { return { x + other.x, y + other.y }; }

            [[nodiscard]]
            constexpr auto operator-(vec2 other) noexcept -> vec2
            { return { x - other.x, y - other.y }; }

            template <typename Up>
            [[nodiscard]]
            constexpr auto operator*(Up scalar) noexcept -> vec2
                requires std::is_arithmetic_v<Up>
            { return { x * scalar, y * scalar }; }

            [[nodiscard]]
            static constexpr auto dot(vec2 first, vec2 second) noexcept -> Tp
            { return (first.x * second.x) + (first.y * second.y); }
        };
    }


    template <typename T>
    using tvec2 = _impl::vec2<T>;
    using vec2  = _impl::vec2<float>;
    using ivec2 = _impl::vec2<int>;
    using uvec2 = _impl::vec2<unsigned>;
}
