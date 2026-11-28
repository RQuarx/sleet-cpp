#pragma once
#include <algorithm>
#include <cmath>

#include "sleet/math/rect.hh"
#include "sleet/math/vec2.hh"


namespace sleet::widget
{
    /// The width of a scrollbar track in logical pixels.
    inline constexpr float scrollbar_thickness = 6.F;

    /// The gap between a scrollbar track and the viewport edge.
    inline constexpr float scrollbar_inset = 2.F;

    /// The shortest scrollbar thumb, so it stays easy to grab.
    inline constexpr float scrollbar_minimum_thumb = 24.F;

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


    /// One scrollbar track and thumb in logical coordinates.
    struct scrollbar_axis
    {
        bool       visible = false;
        math::rect track;
        math::rect thumb;
    };


    /// The scrollbars painted along the right and bottom edges of a viewport.
    struct scrollbar_layout
    {
        scrollbar_axis vertical;
        scrollbar_axis horizontal;
    };


    /// Computes the scrollbar tracks and thumbs for a viewport. The runtime
    /// uses the same geometry for hit testing and dragging that the renderer
    /// uses for painting, so the two cannot drift apart.
    /// \param bounds The scrollable node's border box in logical pixels.
    /// \param metrics The clamped scroll state for the viewport.
    /// \param thickness The scrollbar track thickness.
    /// \param inset The gap between the track and the viewport edge.
    /// \param minimum The minimum thumb length.
    /// \return The visible axes and their logical rectangles.
    [[nodiscard]]
    inline auto scrollbar_layout_for(const math::rect     &bounds,
                                     const scroll_metrics &metrics,
                                     float                 thickness = scrollbar_thickness,
                                     float                 inset     = scrollbar_inset,
                                     float minimum = scrollbar_minimum_thumb) noexcept
        -> scrollbar_layout
    {
        auto result = scrollbar_layout {};

        if (!std::isfinite(bounds.size.x) or !std::isfinite(bounds.size.y)
            or bounds.size.x <= thickness or bounds.size.y <= thickness)
            return result;

        const auto vertical   = metrics.max.y > 0.5F;
        const auto horizontal = metrics.max.x > 0.5F;

        if (vertical)
        {
            const auto track = math::rect {
                bounds.right() - inset - thickness,
                bounds.position.y + inset,
                thickness,
                bounds.size.y - (2.F * inset),
            };

            if (track.size.y > 0.F)
            {
                const auto thumb = scrollbar_thumb_geometry(
                    track.size.y, track.size.y + metrics.max.y, metrics.offset.y, minimum);

                if (thumb.length > 0.F)
                    result.vertical = {
                        true,
                        track,
                        { track.position.x, track.position.y + thumb.start, thickness,
                          thumb.length },
                    };
            }
        }

        if (horizontal)
        {
            const auto track = math::rect {
                bounds.position.x + inset,
                bounds.bottom() - inset - thickness,
                bounds.size.x - (2.F * inset) - (vertical ? thickness + inset : 0.F),
                thickness,
            };

            if (track.size.x > 0.F)
            {
                const auto thumb = scrollbar_thumb_geometry(
                    track.size.x, track.size.x + metrics.max.x, metrics.offset.x, minimum);

                if (thumb.length > 0.F)
                    result.horizontal = {
                        true,
                        track,
                        { track.position.x + thumb.start, track.position.y, thumb.length,
                          thickness },
                    };
            }
        }

        return result;
    }
}
