#pragma once
#include <cstddef>
#include <stdexcept>


namespace sleet::math
{
    template <typename Tp>
    struct basic_vec2
    {
        Tp x {};
        Tp y {};


        constexpr basic_vec2() noexcept = default;
        constexpr basic_vec2(Tp all) noexcept : x { all }, y { all } {}
        constexpr basic_vec2(Tp first, Tp second) noexcept : x { first }, y { second } {}


        [[nodiscard]]
        constexpr auto operator[](std::size_t index) -> Tp &
        {
            if (index == 0) return x;
            if (index == 1) return y;
            throw std::out_of_range { "vec2 index must be 0 or 1" };
        }

        [[nodiscard]]
        constexpr auto operator[](std::size_t index) const -> const Tp &
        {
            if (index == 0) return x;
            if (index == 1) return y;
            throw std::out_of_range { "vec2 index must be 0 or 1" };
        }


        [[nodiscard]]
        constexpr auto operator+(basic_vec2 other) const noexcept -> basic_vec2
        { return { x + other.x, y + other.y }; }

        [[nodiscard]]
        constexpr auto operator-(basic_vec2 other) const noexcept -> basic_vec2
        { return { x - other.x, y - other.y }; }

        [[nodiscard]]
        constexpr auto operator*(Tp scalar) const noexcept -> basic_vec2
        { return { x * scalar, y * scalar }; }

        [[nodiscard]]
        constexpr auto operator/(Tp scalar) const noexcept -> basic_vec2
        { return { x / scalar, y / scalar }; }

        constexpr auto operator+=(basic_vec2 other) noexcept -> basic_vec2 &
        {
            x += other.x;
            y += other.y;
            return *this;
        }

        constexpr auto operator-=(basic_vec2 other) noexcept -> basic_vec2 &
        {
            x -= other.x;
            y -= other.y;
            return *this;
        }


        [[nodiscard]]
        static constexpr auto dot(basic_vec2 first, basic_vec2 second) noexcept -> Tp
        { return (first.x * second.x) + (first.y * second.y); }

        [[nodiscard]]
        constexpr auto length_squared() const noexcept -> Tp
        { return dot(*this, *this); }

        [[nodiscard]]
        constexpr auto is_zero() const noexcept -> bool
        { return x == Tp {} and y == Tp {}; }

        [[nodiscard]]
        friend constexpr auto operator==(basic_vec2, basic_vec2) noexcept -> bool = default;
    };


    template <typename T>
    using vec2_of = basic_vec2<T>;

    using vec2  = basic_vec2<float>;
    using ivec2 = basic_vec2<int>;
    using uvec2 = basic_vec2<unsigned>;
}
