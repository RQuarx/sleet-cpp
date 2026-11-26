#pragma once
#include <algorithm>
#include <cmath>

#include "sleet/math/vec2.hh"


namespace sleet::widget
{
    /// The runtime scroll state of one viewport. \c offset is the clamped
    /// current pan, and \c max is the largest pan along each axis.
    struct scroll_metrics
    {
        math::vec2 offset;
        math::vec2 max;
        math::vec2 viewport;
    };


    /// The position and length of a scrollbar thumb along one axis.
    struct scrollbar_thumb
    {
        float start  = 0.F;
        float length = 0.F;
    };


    /// Computes a proportional scrollbar thumb.
    /// \param viewport The visible length in logical pixels.
    /// \param content The total content length in logical pixels.
    /// \param offset The current scroll offset.
    /// \param minimum The minimum thumb length.
    /// \return The thumb start and length, or an empty thumb when the content
    /// fits.
    [[nodiscard]]
    inline auto scrollbar_thumb_geometry(float viewport,
                                         float content,
                                         float offset,
                                         float minimum = 24.F) noexcept -> scrollbar_thumb
    {
        if (!std::isfinite(viewport) or !std::isfinite(content) or viewport <= 0.F
            or content <= viewport)
            return {};

        const auto length = std::min(viewport, std::max(minimum, viewport * (viewport / content)));
        const auto travel = std::max(0.F, content - viewport);
        const auto amount = travel > 0.F ? std::clamp(offset / travel, 0.F, 1.F) : 0.F;

        return { amount * (viewport - length), length };
    }
}
