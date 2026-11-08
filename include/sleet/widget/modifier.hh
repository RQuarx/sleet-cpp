#pragma once
#include "sleet/widget/node.hh"


namespace sleet::widget
{
#define MODIFIER(name, type, member)                                           \
    inline auto name(type v)                                                   \
    {                                                                          \
        return _impl::modifier_fn { [v = std::move(v)](_impl::node_data &data) \
                                    { data.member = v; } };                    \
    }

    MODIFIER(id, widget_id, id)
    MODIFIER(gap, float, gap)

    MODIFIER(halign, align, alignment.horizontal)
    MODIFIER(valign, align, alignment.vertical)

#undef MODIFIER
#define MODIFIER(name, member, on_true, on_false)                                      \
    inline auto name(bool val)                                                         \
    {                                                                                  \
        return _impl::modifier_fn { [val](_impl::node_data &data)                      \
                                    { data.member = val ? (on_true) : (on_false); } }; \
    }

    MODIFIER(hexpand, size.horizontal, sizing::grow(), sizing::fit())
    MODIFIER(vexpand, size.vertical, sizing::grow(), sizing::fit())

#undef MODIFIER

    inline auto width(float px)
    {
        return _impl::modifier_fn { [=](_impl::node_data &data)
                                    { data.size.w = sizing::fixed(px); } };
    }

    inline auto height(float px)
    {
        return _impl::modifier_fn { [=](_impl::node_data &data)
                                    { data.size.h = sizing::fixed(px); } };
    }


    inline auto size(float width, float height)
    {
        return _impl::modifier_fn { [=](_impl::node_data &data)
                                    {
                                        data.size.w = sizing::fixed(width);
                                        data.size.h = sizing::fixed(height);
                                    } };
    }


    inline auto size(float all)
    {
        return _impl::modifier_fn { [=](_impl::node_data &data)
                                    {
                                        data.size.w = sizing::fixed(all);
                                        data.size.h = sizing::fixed(all);
                                    } };
    }


    inline auto size(sizing width, sizing height)
    {
        return _impl::modifier_fn { [=](_impl::node_data &data)
                                    {
                                        data.size.w = width;
                                        data.size.h = height;
                                    } };
    }

    inline auto size(sizing s)
    {
        return _impl::modifier_fn { [=](_impl::node_data &data)
                                    {
                                        data.size.w = s;
                                        data.size.h = s;
                                    } };
    }


    inline auto text_of(std::string label)
    {
        return _impl::modifier_fn { [l = std::move(label)](_impl::node_data &data)
                                    { data.text = l; } };
    }


    inline auto text_weight(font_weight w)
    {
        return _impl::modifier_fn { [=](_impl::node_data &d) { d.font.weight = w; } };
    }


    inline auto text_decoration(text_attribute::flag f, bool state = true)
    {
        return _impl::modifier_fn { [=](_impl::node_data &d) { d.font.attribute.set(f, state); } };
    }

    inline auto margin(math::vec4 margin)
    {
        return _impl::modifier_fn { [=](_impl::node_data &d) { d.margin = margin; } };
    }

    inline auto padding(math::vec4 padding)
    {
        return _impl::modifier_fn { [=](_impl::node_data &d) { d.padding = padding; } };
    }
}
