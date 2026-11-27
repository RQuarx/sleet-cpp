#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

#include <kei-utils/bitmask.hh>

#include "sleet/color.hh"


namespace sleet::widget
{
    /// How a widget takes up space along one axis.
    struct sizing
    {
        enum class mode : std::uint8_t
        {
            fit,   ///< Use the intrinsic size.
            grow,  ///< Share free space on the parent's main axis using \c flex.
            fill,  ///< Stretch across the parent's cross axis and share its main axis.
            fixed, ///< Use an explicit size.
        };


        [[nodiscard]]
        static constexpr auto fit() noexcept -> sizing
        { return { mode::fit, 0.F }; }

        [[nodiscard]]
        static constexpr auto grow() noexcept -> sizing
        { return { mode::grow, 1.F }; }

        [[nodiscard]]
        static constexpr auto fill() noexcept -> sizing
        { return { mode::fill, 1.F }; }

        [[nodiscard]]
        static constexpr auto fixed(float value) noexcept -> sizing
        {
            if (!std::isfinite(value) or value < 0.F) return { mode::fixed, 0.F };
            return { mode::fixed, value };
        }


        [[nodiscard]]
        constexpr auto is_fit() const noexcept -> bool
        { return m_mode == mode::fit; }
        [[nodiscard]]
        constexpr auto is_grow() const noexcept -> bool
        { return m_mode == mode::grow; }
        [[nodiscard]]
        constexpr auto is_fill() const noexcept -> bool
        { return m_mode == mode::fill; }
        [[nodiscard]]
        constexpr auto is_fixed() const noexcept -> bool
        { return m_mode == mode::fixed; }

        [[nodiscard]]
        constexpr auto get_mode() const noexcept -> mode
        { return m_mode; }

        [[nodiscard]]
        constexpr auto get_fixed_size() const noexcept -> float
        { return m_mode == mode::fixed ? m_value : 0.F; }


        [[nodiscard]]
        friend constexpr auto operator==(sizing, sizing) noexcept -> bool = default;

    private:
        constexpr sizing(mode kind, float value) noexcept : m_mode { kind }, m_value { value } {}


        mode  m_mode  = mode::fit;
        float m_value = 0.F;
    };


    enum class align : std::uint8_t
    {
        start,
        center,
        end,
    };


    enum class justify : std::uint8_t
    {
        start,
        center,
        end,
        between,
    };


    /// How a text node breaks lines when its width is constrained.
    enum class text_wrap_mode : std::uint8_t
    {
        no_wrap,      ///< Keep every paragraph on one line; overflow is clipped.
        wrap,         ///< Break at Unicode line-break opportunities.
        wrap_anywhere ///< Also break inside a word when the word itself is too wide.
    };


    /// Horizontal alignment for text inside its content box. \c justify
    /// expands spaces so all but the last line of a paragraph reach both edges.
    enum class text_alignment : std::uint8_t
    {
        left,
        center,
        right,
        justify,
    };


    /// Weights use CSS and OpenType numbers, so the underlying type can hold
    /// intermediate values such as 550.
    enum class font_weight : std::uint16_t
    {
        thin        = 100,
        extra_light = 200,
        light       = 300,
        normal      = 400,
        medium      = 500,
        semibold    = 600,
        bold        = 700,
        extra_bold  = 800,
        black       = 900,
    };


    class text_attribute final : public kei::bitmask<std::uint8_t>
    {
    public:
        enum flag : base_type
        {
            underline     = 0b001,
            strikethrough = 0b010,
            italic        = 0b100,
        };
    };


    enum class font_role : std::uint8_t
    {
        body,
        heading,
        caption,
        code,
    };


    struct font_spec
    {
        font_role      role = font_role::body;
        text_attribute attribute;

        /// \c std::nullopt falls back to the role's default weight.
        std::optional<font_weight> weight;

        /// \c std::nullopt falls back to the role's default size.
        std::optional<float> size;
    };


    /// Visual properties that are independent of layout.
    struct style
    {
        std::optional<color> background;
        std::optional<color> foreground;
        std::optional<color> border;
        float                border_width = 1.F;

        /// Corner radius in logical pixels. \c std::nullopt asks the renderer
        /// for the theme's default control radius.
        std::optional<float> corner_radius;
    };
}
