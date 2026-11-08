#pragma once
#include <array>

#include "sleet/math/vec2.hh"


namespace sleet::math
{
    namespace _impl
    {
        template <typename Tp>
        struct vec4
        {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-anonymous-struct"
#pragma GCC diagnostic ignored "-Wnested-anon-types"
            /* clang-format off */
            union
            {
                std::array<Tp, 4> data;
                struct { Tp x, y, w, h; };
                struct { Tp a, b, c, d; };
                struct { Tp top, end, bottom, begin; };
            };
            /* clang-format on */
#pragma GCC diagnostic pop

            constexpr vec4(Tp all) noexcept : x { all }, y { all }, w { all }, h { all } {}
            constexpr vec4(Tp first, Tp second, Tp third, Tp fourth) noexcept
                : x { first }, y { second }, w { third }, h { fourth }
            {
            }

            constexpr vec4(vec2<Tp> first, vec2<Tp> second) noexcept
                : x { first.x }, y { first.y }, w { second.w }, h { second.h }
            {
            }
        };
    }


    using vec4  = _impl::vec4<float>;
    using ivec4 = _impl::vec4<int>;
    using uvec4 = _impl::vec4<unsigned>;
}
