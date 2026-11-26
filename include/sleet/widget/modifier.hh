#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include "sleet/widget/node.hh"


namespace sleet::widget
{
    namespace _impl
    {
        template <typename F>
        [[nodiscard]]
        auto make_modifier(F &&fn)
        { return modifier_fn<std::decay_t<F>> { std::forward<F>(fn) }; }
    }


    [[nodiscard]]
    inline auto id(widget_id value)
    {
        return _impl::make_modifier([value = std::move(value)](node_data &data)
                                    { data.id = value; });
    }


    /// An explicit, stable key. Identical to \c id and easier to read at call
    /// sites that are built in loops.
    [[nodiscard]]
    inline auto key(widget_id value)
    { return id(std::move(value)); }

    [[nodiscard]]
    inline auto gap(float value)
    {
        return _impl::make_modifier([value](node_data &data) { data.gap = value; });
    }


    [[nodiscard]]
    inline auto halign(align value)
    {
        return _impl::make_modifier([value](node_data &data) { data.alignment.x = value; });
    }

    [[nodiscard]]
    inline auto valign(align value)
    {
        return _impl::make_modifier([value](node_data &data) { data.alignment.y = value; });
    }

    [[nodiscard]]
    inline auto align_self(align value)
    {
        return _impl::make_modifier([value](node_data &data) { data.align_self = value; });
    }

    [[nodiscard]]
    inline auto justify_content(justify value)
    {
        return _impl::make_modifier([value](node_data &data) { data.main_axis = value; });
    }


    [[nodiscard]]
    inline auto hexpand(bool value = true)
    {
        return _impl::make_modifier([value](node_data &data)
                                    { data.size.x = value ? sizing::fill() : sizing::fit(); });
    }

    [[nodiscard]]
    inline auto vexpand(bool value = true)
    {
        return _impl::make_modifier([value](node_data &data)
                                    { data.size.y = value ? sizing::fill() : sizing::fit(); });
    }

    [[nodiscard]]
    inline auto flex(float weight)
    {
        return _impl::make_modifier([weight](node_data &data) { data.flex = weight; });
    }


    [[nodiscard]]
    inline auto width(float px)
    {
        return _impl::make_modifier([px](node_data &data) { data.size.x = sizing::fixed(px); });
    }

    [[nodiscard]]
    inline auto width(sizing value)
    {
        return _impl::make_modifier([value](node_data &data) { data.size.x = value; });
    }

    [[nodiscard]]
    inline auto height(float px)
    {
        return _impl::make_modifier([px](node_data &data) { data.size.y = sizing::fixed(px); });
    }

    [[nodiscard]]
    inline auto height(sizing value)
    {
        return _impl::make_modifier([value](node_data &data) { data.size.y = value; });
    }

    [[nodiscard]]
    inline auto size(float width, float height)
    {
        return _impl::make_modifier(
            [width, height](node_data &data)
            {
                data.size.x = sizing::fixed(width);
                data.size.y = sizing::fixed(height);
            });
    }

    [[nodiscard]]
    inline auto size(float all)
    {
        return _impl::make_modifier(
            [all](node_data &data)
            {
                data.size.x = sizing::fixed(all);
                data.size.y = sizing::fixed(all);
            });
    }

    [[nodiscard]]
    inline auto size(sizing width, sizing height)
    {
        return _impl::make_modifier(
            [width, height](node_data &data)
            {
                data.size.x = width;
                data.size.y = height;
            });
    }

    [[nodiscard]]
    inline auto size(sizing value)
    {
        return _impl::make_modifier(
            [value](node_data &data)
            {
                data.size.x = value;
                data.size.y = value;
            });
    }


    [[nodiscard]]
    inline auto margin(math::edge_insets value)
    {
        return _impl::make_modifier([value = value.sanitized()](node_data &data)
                                    { data.margin = value; });
    }

    [[nodiscard]]
    inline auto margin(float all)
    { return margin(math::edge_insets { all }); }

    [[nodiscard]]
    inline auto margin(float vertical, float horizontal)
    { return margin(math::edge_insets { vertical, horizontal }); }

    [[nodiscard]]
    inline auto padding(math::edge_insets value)
    {
        return _impl::make_modifier([value = value.clamped_non_negative()](node_data &data)
                                    { data.padding = value; });
    }

    [[nodiscard]]
    inline auto padding(float all)
    { return padding(math::edge_insets { all }); }

    [[nodiscard]]
    inline auto padding(float vertical, float horizontal)
    { return padding(math::edge_insets { vertical, horizontal }); }


    [[nodiscard]]
    inline auto text_of(std::string text)
    {
        return _impl::make_modifier([text = std::move(text)](node_data &data)
                                    { data.text = text; });
    }

    [[nodiscard]]
    inline auto text_weight(font_weight weight)
    {
        return _impl::make_modifier([weight](node_data &data) { data.font.weight = weight; });
    }

    [[nodiscard]]
    inline auto text_size(float size)
    {
        return _impl::make_modifier([size](node_data &data) { data.font.size = size; });
    }

    [[nodiscard]]
    inline auto text_role(font_role role)
    {
        return _impl::make_modifier([role](node_data &data) { data.font.role = role; });
    }

    [[nodiscard]]
    inline auto text_decoration(text_attribute::flag flag, bool state = true)
    {
        return _impl::make_modifier([flag, state](node_data &data)
                                    { data.font.attribute.set(flag, state); });
    }


    [[nodiscard]]
    inline auto background(color value)
    {
        return _impl::make_modifier([value](node_data &data) { data.visual.background = value; });
    }

    [[nodiscard]]
    inline auto foreground(color value)
    {
        return _impl::make_modifier([value](node_data &data) { data.visual.foreground = value; });
    }

    [[nodiscard]]
    inline auto text_color(color value)
    { return foreground(value); }

    [[nodiscard]]
    inline auto border(color value, float width = 1.F)
    {
        return _impl::make_modifier(
            [value, width](node_data &data)
            {
                data.visual.border       = value;
                data.visual.border_width = width;
            });
    }

    [[nodiscard]]
    inline auto border_width(float width)
    {
        return _impl::make_modifier([width](node_data &data) { data.visual.border_width = width; });
    }


    /// Sets the corner radius in logical pixels. Values are clamped to zero.
    [[nodiscard]]
    inline auto radius(float value)
    {
        return _impl::make_modifier(
            [value](node_data &data)
            { data.visual.corner_radius = std::isfinite(value) ? std::max(0.F, value) : 0.F; });
    }

    [[nodiscard]]
    inline auto corner_radius(float value)
    { return radius(value); }


    [[nodiscard]]
    inline auto on_click(std::string action)
    {
        return _impl::make_modifier([action = std::move(action)](node_data &data)
                                    { data.action = action; });
    }

    [[nodiscard]]
    inline auto visible(bool value = true)
    {
        return _impl::make_modifier([value](node_data &data) { data.visible = value; });
    }


    /// Takes a node out of its parent's flow. \c anchor_at selects the parent
    /// corner or edge, and \c placement adds an offset from it.
    [[nodiscard]]
    inline auto floating(bool value = true)
    {
        return _impl::make_modifier([value](node_data &data) { data.floating = value; });
    }

    /// Positions a floating node against an edge or corner of its parent's
    /// content box.
    [[nodiscard]]
    inline auto anchor_at(anchor_point value)
    {
        return _impl::make_modifier([value](node_data &data) { data.placement_anchor = value; });
    }

    [[nodiscard]]
    inline auto placement(math::vec2 value)
    {
        return _impl::make_modifier([value](node_data &data) { data.placement = value; });
    }

    [[nodiscard]]
    inline auto placement(float x, float y)
    { return placement(math::vec2 { x, y }); }

    [[nodiscard]]
    inline auto offset(float x, float y)
    { return placement(x, y); }

    /// Controls paint and hit-test order among floating siblings. Higher values
    /// are painted later and tested first.
    [[nodiscard]]
    inline auto z_index(int value)
    {
        return _impl::make_modifier([value](node_data &data) { data.z_index = value; });
    }

    /// A readable alias for \c z_index.
    [[nodiscard]]
    inline auto stacking(int value)
    { return z_index(value); }


    [[nodiscard]]
    inline auto clip(bool value = true)
    {
        return _impl::make_modifier([value](node_data &data) { data.clip_children = value; });
    }

    [[nodiscard]]
    inline auto clip_children(bool value = true)
    { return clip(value); }


    /// Makes a clipped node respond to wheel scrolling. The runtime clamps the
    /// offset to the measured content bounds.
    [[nodiscard]]
    inline auto scrollable(bool value = true)
    {
        return _impl::make_modifier([value](node_data &data) { data.scrollable = value; });
    }


    [[nodiscard]]
    inline auto disabled(bool value = true)
    {
        return _impl::make_modifier([value](node_data &data) { data.disabled = value; });
    }


    [[nodiscard]]
    inline auto control(control_kind value)
    {
        return _impl::make_modifier([value](node_data &data) { data.control = value; });
    }

    [[nodiscard]]
    inline auto tone_of(tone value)
    {
        return _impl::make_modifier([value](node_data &data) { data.tint = value; });
    }

    [[nodiscard]]
    inline auto variant(button_variant value)
    {
        return _impl::make_modifier([value](node_data &data) { data.variant = value; });
    }

    [[nodiscard]]
    inline auto focusable(bool value = true)
    {
        return _impl::make_modifier([value](node_data &data) { data.focusable = value; });
    }

    [[nodiscard]]
    inline auto checked(bool value = true)
    {
        return _impl::make_modifier([value](node_data &data) { data.checked = value; });
    }

    [[nodiscard]]
    inline auto control_value(float value)
    {
        return _impl::make_modifier([value](node_data &data)
                                    { data.control_value = std::clamp(value, 0.F, 1.F); });
    }

    [[nodiscard]]
    inline auto placeholder(std::string value)
    {
        return _impl::make_modifier([value = std::move(value)](node_data &data)
                                    { data.placeholder = value; });
    }


    /// Eases interactive feedback and explicit style changes over \p duration
    /// seconds. A zero duration makes the widget instant.
    [[nodiscard]]
    inline auto transition(float duration, easing curve = easing::ease_out)
    {
        return _impl::make_modifier(
            [duration, curve](node_data &data)
            {
                data.motion.duration = std::isfinite(duration) ? std::max(0.F, duration) : 0.F;
                data.motion.curve    = curve;
            });
    }

    [[nodiscard]]
    inline auto transition(transition_spec spec)
    {
        return _impl::make_modifier([spec](node_data &data) { data.motion = spec; });
    }

    /// Removes any inherited transition from this widget.
    [[nodiscard]]
    inline auto no_transition()
    { return transition(0.F); }
}
