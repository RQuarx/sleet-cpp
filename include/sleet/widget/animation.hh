#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>


namespace sleet::widget
{
    /// An easing curve for animated properties.
    enum class easing : std::uint8_t
    {
        linear,
        ease_in,
        ease_out,
        ease_in_out,
        ease_out_back,
    };


    /// Maps a normalized progress in \c [0, 1] through an easing curve.
    /// \param curve The curve to apply.
    /// \param progress The linear progress.
    /// \return The eased progress. Values stay in \c [0, 1] except for
    /// \c ease_out_back, which may overshoot slightly near the end.
    [[nodiscard]]
    inline auto ease(easing curve, float progress) noexcept -> float
    {
        const auto t = std::clamp(std::isfinite(progress) ? progress : 0.F, 0.F, 1.F);

        switch (curve)
        {
        case easing::linear:  return t;
        case easing::ease_in: return t * t * t;
        case easing::ease_out:
            {
                const auto inverse = 1.F - t;
                return 1.F - (inverse * inverse * inverse);
            }
        case easing::ease_in_out:
            {
                if (t < 0.5F) return 4.F * t * t * t;
                const auto inverse = -2.F * t + 2.F;
                return 1.F - ((inverse * inverse * inverse) / 2.F);
            }
        case easing::ease_out_back:
            {
                constexpr auto c1      = 1.70158F;
                constexpr auto c3      = c1 + 1.F;
                const auto     inverse = t - 1.F;
                return 1.F + (c3 * inverse * inverse * inverse) + (c1 * inverse * inverse);
            }
        }

        return t;
    }


    /// How a visual property eases toward a new value.
    struct transition_spec
    {
        /// Duration in seconds. Zero disables the transition.
        float  duration = 0.F;
        easing curve    = easing::ease_out;


        [[nodiscard]]
        friend constexpr auto operator==(const transition_spec &, const transition_spec &) noexcept
            -> bool = default;
    };


    /// A scalar value that eases toward its target. The linear progress is
    /// stored separately from the eased value so changing the target mid-flight
    /// restarts cleanly from the currently visible value.
    class animated_value
    {
    public:
        animated_value() = default;
        explicit animated_value(float value) noexcept { snap(value); }


        /// Jumps to \p value without animating.
        void snap(float value) noexcept
        {
            m_current  = value;
            m_start    = value;
            m_target   = value;
            m_progress = 1.F;
        }


        /// Starts easing toward \p value when it differs from the target.
        void set_target(float value) noexcept
        {
            if (!std::isfinite(value) or value == m_target) return;

            m_start    = m_current;
            m_target   = value;
            m_progress = 0.F;
        }


        /// Advances the transition.
        /// \param delta_seconds Time since the previous frame.
        /// \param duration The transition duration in seconds.
        /// \param curve The easing curve.
        /// \return \c true while the value is still moving.
        auto advance(float delta_seconds, float duration, easing curve) noexcept -> bool
        {
            if (settled()) return false;

            if (!std::isfinite(duration) or duration <= 0.F or !std::isfinite(delta_seconds))
            {
                snap(m_target);
                return false;
            }

            m_progress = std::min(1.F, m_progress + (std::max(0.F, delta_seconds) / duration));
            m_current  = std::lerp(m_start, m_target, ease(curve, m_progress));

            if (m_progress >= 1.F) m_current = m_target;
            return true;
        }


        [[nodiscard]] auto current() const noexcept -> float { return m_current; }
        [[nodiscard]] auto target() const noexcept -> float { return m_target; }

        [[nodiscard]]
        auto settled() const noexcept -> bool
        { return m_progress >= 1.F or m_current == m_target; }

    private:
        float m_current  = 0.F;
        float m_start    = 0.F;
        float m_target   = 0.F;
        float m_progress = 1.F;
    };
}
