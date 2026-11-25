#pragma once
#include <cstddef>
#include <stdexcept>


namespace sleet::math
{
    /// A plain four-component vector. For margins and padding, use
    /// \c math::edge_insets instead.
    template <typename Tp>
    struct basic_vec4
    {
        Tp x {};
        Tp y {};
        Tp z {};
        Tp w {};


        constexpr basic_vec4() noexcept = default;
        constexpr basic_vec4(Tp all) noexcept : x { all }, y { all }, z { all }, w { all } {}
        constexpr basic_vec4(Tp first, Tp second, Tp third, Tp fourth) noexcept
            : x { first }, y { second }, z { third }, w { fourth }
        {
        }


        [[nodiscard]]
        constexpr auto operator[](std::size_t index) -> Tp &
        {
            switch (index)
            {
            case 0:  return x;
            case 1:  return y;
            case 2:  return z;
            case 3:  return w;
            default: throw std::out_of_range { "vec4 index must be in [0, 4)" };
            }
        }

        [[nodiscard]]
        constexpr auto operator[](std::size_t index) const -> const Tp &
        {
            switch (index)
            {
            case 0:  return x;
            case 1:  return y;
            case 2:  return z;
            case 3:  return w;
            default: throw std::out_of_range { "vec4 index must be in [0, 4)" };
            }
        }


        [[nodiscard]]
        constexpr auto operator+(basic_vec4 other) const noexcept -> basic_vec4
        { return { x + other.x, y + other.y, z + other.z, w + other.w }; }

        [[nodiscard]]
        constexpr auto operator-(basic_vec4 other) const noexcept -> basic_vec4
        { return { x - other.x, y - other.y, z - other.z, w - other.w }; }

        [[nodiscard]]
        constexpr auto operator*(Tp scalar) const noexcept -> basic_vec4
        { return { x * scalar, y * scalar, z * scalar, w * scalar }; }

        [[nodiscard]]
        friend constexpr auto operator==(basic_vec4, basic_vec4) noexcept -> bool = default;
    };


    template <typename T>
    using vec4_of = basic_vec4<T>;

    using vec4  = basic_vec4<float>;
    using ivec4 = basic_vec4<int>;
    using uvec4 = basic_vec4<unsigned>;
}
