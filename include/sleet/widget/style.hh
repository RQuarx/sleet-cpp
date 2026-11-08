#pragma once
#include <cstdint>
#include <optional>

#include <kei-utils/bitmask.hh>


namespace sleet::widget
{
    /// Describes the size of a widget.
    struct sizing
    {
        [[nodiscard]] static constexpr auto fit() noexcept -> sizing { return -1.F; }
        [[nodiscard]] static constexpr auto grow() noexcept -> sizing { return -2.F; }
        [[nodiscard]] static constexpr auto fixed(float v) noexcept -> sizing { return v; }

        [[nodiscard]] auto is_fit() const noexcept -> bool { return m_value == -1.F; }
        [[nodiscard]] auto is_grow() const noexcept -> bool { return m_value == -2.F; }
        [[nodiscard]] auto is_fixed() const noexcept -> bool { return m_value >= 0.F; }

        [[nodiscard]] auto get_fixed_size() const noexcept -> float { return m_value; }

    private:
        float m_value = -1.F;


        constexpr sizing(float val) noexcept : m_value { val } {}
    };


    enum class align : std::uint8_t
    {
        start,
        center,
        end,
    };


    /// CSS/OpenType numbers, so 550 is representable
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
            underline     = 1 << 0,
            strikethrough = 1 << 1,
            italic        = 1 << 2,
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

        /// unset = the role's default weight
        std::optional<font_weight> weight;
    };
}
