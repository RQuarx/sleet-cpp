#pragma once
#include <algorithm>
#include <cmath>


namespace sleet::math
{
    /// Edge insets in CSS order: top, right, bottom, left.
    struct edge_insets
    {
        float top    = 0.F;
        float right  = 0.F;
        float bottom = 0.F;
        float left   = 0.F;


        constexpr edge_insets() noexcept = default;
        constexpr explicit edge_insets(float all) noexcept
            : top { all }, right { all }, bottom { all }, left { all }
        {
        }
        constexpr edge_insets(float vertical, float horizontal) noexcept
            : top { vertical }, right { horizontal }, bottom { vertical }, left { horizontal }
        {
        }
        constexpr edge_insets(float top, float right, float bottom, float left) noexcept
            : top { top }, right { right }, bottom { bottom }, left { left }
        {
        }


        [[nodiscard]]
        constexpr auto horizontal() const noexcept -> float
        { return left + right; }

        [[nodiscard]]
        constexpr auto vertical() const noexcept -> float
        { return top + bottom; }

        [[nodiscard]]
        constexpr auto is_zero() const noexcept -> bool
        { return top == 0.F and right == 0.F and bottom == 0.F and left == 0.F; }

        /// Replaces non-finite values with zero but keeps negative finite
        /// margins. Use this for user-supplied metrics that must not poison
        /// layout.
        [[nodiscard]]
        constexpr auto sanitized() const noexcept -> edge_insets
        {
            const auto clean
                = [](float value) noexcept { return std::isfinite(value) ? value : 0.F; };
            return { clean(top), clean(right), clean(bottom), clean(left) };
        }

        [[nodiscard]]
        constexpr auto clamped_non_negative() const noexcept -> edge_insets
        {
            const auto clean = [](float value) noexcept
            { return std::isfinite(value) ? std::max(0.F, value) : 0.F; };
            return { clean(top), clean(right), clean(bottom), clean(left) };
        }

        [[nodiscard]]
        friend constexpr auto operator==(edge_insets, edge_insets) noexcept -> bool = default;
    };
}
