#include <algorithm>
#include <limits>
#include <optional>
#include <ranges>
#include <unordered_map>

#include "sleet/widget/layout.hh"


namespace
{
    using sleet::math::rect;
    using sleet::math::vec2;
    using sleet::math::vec4;
    using sleet::widget::node_kind;
    using sleet::widget::sizing;
    using sleet::widget::text_measurer;
    using sleet::widget::_impl::node_data;


    /// Tolerance used when comparing accumulated float sizes.
    constexpr auto epsilon = std::numeric_limits<float>::epsilon();


    struct measured
    {
        static constexpr auto uninitialized = std::numeric_limits<float>::quiet_NaN();


        std::vector<measured> children;

        measured *parent = nullptr;

        /// Content based preferred size (padding included). Fixed sizes override it.
        vec2 intrinsic_size { uninitialized };

        /// Final size. Starts at the intrinsic size, `grow` nodes are then expanded/shrunk.
        vec2 size { uninitialized };
        vec2 pos { uninitialized };


        static auto build_from_widget_node(const node_data &widget) -> measured
        {
            measured m;
            m.children.reserve(widget.children.size());

            for (const auto &child : widget.children)
                m.children.emplace_back(measured::build_from_widget_node(child));

            return m;
        }

        void set_parents(measured *p = nullptr)
        {
            parent = p;
            for (auto &child : children) child.set_parents(this);
        }

        [[nodiscard]] auto to_rect() const noexcept -> rect { return { pos, size }; }
    };


    enum axis : std::uint8_t
    {
        width  = 0,
        height = 1,
    };

    auto combine_axis(vec4 v, axis a) noexcept -> float
    { return a == axis::width ? v.begin + v.end : v.top + v.bottom; }

    auto leading_axis(vec4 v, axis a) noexcept -> float
    { return a == axis::width ? v.begin : v.top; }

    auto is_stacked_axis(const node_data &node, axis a) noexcept -> bool
    { return (node.is_row() and a == axis::width) or (node.is_column() and a == axis::height); }


#pragma region

    /// Extent of the children along axis `a`, based on their intrinsic sizes.
    auto measure_child_content(const node_data &node, const measured &measure_node, axis a) noexcept
        -> float
    {
        float content    = 0;
        bool  is_stacked = is_stacked_axis(node, a);

        for (const auto &[w_child, m_child] : std::views::zip(node.children, measure_node.children))
        {
            float extent = m_child.intrinsic_size.data[a] + combine_axis(w_child.margin, a);
            content      = is_stacked ? content + extent : std::max(content, extent);
        }

        if (is_stacked and !node.children.empty())
            content += static_cast<float>(node.children.size() - 1) * node.gap;
        return content;
    }


    /// Computes this node's own intrinsic size and initialises `size` with it.
    ///
    /// @note The children must already be measured.
    auto apply_intrinsic_size(const node_data     &node,
                              measured            &measure_node,
                              const text_measurer &text_measurer) noexcept -> kei::result<>
    {
        if (node.is_text())
        {
            auto res = text_measurer(node.text, node.font, std::nullopt)
                           .and_then(
                               [&](vec2 text_size)
                               {
                                   measure_node.intrinsic_size = text_size;
                                   for (auto a : { axis::width, axis::height })
                                       measure_node.intrinsic_size.data[a]
                                           += combine_axis(node.padding, a);
                                   return kei::result<> {};
                               });
            if (!res) return res;
        }
        else
        {
            for (auto a : { axis::width, axis::height })
                measure_node.intrinsic_size.data[a]
                    = combine_axis(node.padding, a) + measure_child_content(node, measure_node, a);
        }

        /* A fixed size always wins (this also applies to text nodes). */
        for (auto a : { axis::width, axis::height })
            if (node.size.data[a].is_fixed())
                measure_node.intrinsic_size.data[a] = node.size.data[a].get_fixed_size();

        measure_node.size = measure_node.intrinsic_size;
        return {};
    }


    auto measure_intrinsic(const node_data     &node,
                           measured            &measure_node,
                           const text_measurer &text_measurer) noexcept -> kei::result<>
    {
        for (const auto &[w_child, m_child] : std::views::zip(node.children, measure_node.children))
        {
            for (auto a : { axis::width, axis::height })
                if (node.size.data[a].is_fit() and w_child.size.data[a].is_grow())
                    return kei::error { "A `fit` widget ({}) cannot contain a `grow` child ({}).",
                                        node.id.view(), w_child.id.view() }
                        .unexpected();

            if (auto res = measure_intrinsic(w_child, m_child, text_measurer); !res) return res;
        }

        return apply_intrinsic_size(node, measure_node, text_measurer);
    }
#pragma endregion

#pragma region
    struct grow_slot
    {
        measured *node;

        /// smallest size this child may be shrunk to
        float floor;
    };


    /// Distributes `remaining` (> 0) over the slots, always raising the smallest ones first
    /// until they reach the next level, so sizes end up levelled out.
    void expand_slots(std::vector<grow_slot> &slots, float remaining, axis a) noexcept
    {
        while (remaining > epsilon)
        {
            float smallest = std::numeric_limits<float>::infinity();
            for (const auto &s : slots) smallest = std::min(smallest, s.node->size.data[a]);

            float       next  = std::numeric_limits<float>::infinity();
            std::size_t count = 0;
            for (const auto &s : slots)
            {
                float size = s.node->size.data[a];
                if (size - smallest <= epsilon)
                    count++;
                else
                    next = std::min(next, size);
            }

            float step = std::min(next - smallest, remaining / static_cast<float>(count));

            for (auto &s : slots)
                if (s.node->size.data[a] - smallest <= epsilon) s.node->size.data[a] += step;

            remaining -= step * static_cast<float>(count);
        }
    }


    /// Removes `need` (> 0) from the slots, always cutting the largest ones first, and never
    /// going below a slot's floor. If every slot is at its floor the rest overflows.
    void shrink_slots(std::vector<grow_slot> &slots, float need, axis a) noexcept
    {
        constexpr auto inf = std::numeric_limits<float>::infinity();

        auto shrinkable
            = [a](const grow_slot &s) { return s.node->size.data[a] - s.floor > epsilon; };

        while (need > epsilon)
        {
            float largest = -inf;
            for (const auto &s : slots)
                if (shrinkable(s)) largest = std::max(largest, s.node->size.data[a]);

            if (largest == -inf) return; /* nothing left to shrink */

            float       next        = -inf;
            float       group_floor = -inf;
            std::size_t count       = 0;
            for (const auto &s : slots)
            {
                if (!shrinkable(s)) continue;

                float size = s.node->size.data[a];
                if (largest - size <= epsilon)
                {
                    count++;
                    group_floor = std::max(group_floor, s.floor);
                }
                else
                    next = std::max(next, size);
            }

            float target = std::max(next, group_floor);
            float step   = std::min(largest - target, need / static_cast<float>(count));

            for (auto &s : slots)
                if (shrinkable(s) and largest - s.node->size.data[a] <= epsilon)
                    s.node->size.data[a] = std::max(s.node->size.data[a] - step, s.floor);

            need -= step * static_cast<float>(count);
        }
    }


    auto grow_unstacked_children(const node_data &node, measured &m, axis a) noexcept
        -> kei::result<>
    {
        const float available = m.size.data[a] - combine_axis(node.padding, a);

        for (const auto &[child, measured_child] : std::views::zip(node.children, m.children))
        {
            if (!child.size.data[a].is_grow()) continue;
            if (node.size.data[a].is_fit())
                return kei::error {
                    "A `grow` widget ({}) has a parent ({}) whose sizing mode is `fit`.",
                    child.id.view(), node.id.view()
                }
                    .unexpected();

            measured_child.size.data[a] = std::max(0.F, available - combine_axis(child.margin, a));
        }

        return {};
    }


    void grow_stacked_children(const node_data &node, measured &measure_node, axis a) noexcept
    {
        std::vector<grow_slot> slots;

        /* free space = container - padding - gaps - every child's *current* size and margin */
        float remaining = measure_node.size.data[a] - combine_axis(node.padding, a);

        for (const auto &[child, measured_child] :
             std::views::zip(node.children, measure_node.children))
        {
            remaining -= combine_axis(child.margin, a) + measured_child.size.data[a];

            if (child.size.data[a].is_grow())
                slots.push_back({ &measured_child, combine_axis(child.padding, a) });
        }

        if (node.children.size() > 1)
            remaining -= static_cast<float>(node.children.size() - 1) * node.gap;

        if (slots.empty()) return;

        if (remaining > epsilon)
            expand_slots(slots, remaining, a);
        else if (remaining < -epsilon)
            shrink_slots(slots, -remaining, a);
    }


    auto distribute_space(const node_data &node, measured &m) noexcept -> kei::result<>
    {
        for (auto a : { axis::width, axis::height })
            if (is_stacked_axis(node, a)) { grow_stacked_children(node, m, a); }
            else
            {
                if (auto res = grow_unstacked_children(node, m, a); !res) return res;
            }

        for (const auto &[child, measured_child] : std::views::zip(node.children, m.children))
            if (auto res = distribute_space(child, measured_child); !res) return res;

        return {};
    }
#pragma endregion


    /// Top-left aligned placement: children are stacked along the main axis (with gaps) and
    /// start at the padding edge on the cross axis.
    void place_children(const node_data &node, measured &m)
    {
        for (auto a : { axis::width, axis::height })
        {
            const bool stacked = is_stacked_axis(node, a);
            float      cursor  = m.pos.data[a] + leading_axis(node.padding, a);

            for (const auto &[child, measured_child] : std::views::zip(node.children, m.children))
            {
                const float lead  = leading_axis(child.margin, a);
                const float trail = combine_axis(child.margin, a) - lead;

                measured_child.pos.data[a] = cursor + lead;

                if (stacked) cursor += lead + measured_child.size.data[a] + trail + node.gap;
            }
        }

        for (const auto &[child, measured_child] : std::views::zip(node.children, m.children))
            place_children(child, measured_child);
    }


    void fill_rects(std::unordered_map<std::string_view, rect> &rects,
                    const node_data                            &node,
                    const measured                             &m)
    {
        rects.emplace(node.id.view(), m.to_rect());

        for (const auto &[node_child, measured_child] : std::views::zip(node.children, m.children))
            fill_rects(rects, node_child, measured_child);
    }
}


auto sleet::widget::calculate_layout(const _impl::node_data &root,
                                     const text_measurer    &text_measurer) noexcept
    -> kei::result<std::unordered_map<std::string_view, math::rect>>
{
    auto measured_tree = measured::build_from_widget_node(root);
    measured_tree.set_parents();

    if (auto res = measure_intrinsic(root, measured_tree, text_measurer); !res)
        return std::move(res.error()).unexpected();

    if (auto res = distribute_space(root, measured_tree); !res)
        return std::move(res.error()).unexpected();

    for (auto a : { axis::width, axis::height }) measured_tree.pos.data[a] = 0;
    place_children(root, measured_tree);

    std::unordered_map<std::string_view, math::rect> res;
    res.reserve(root.children_amount() + 1);

    fill_rects(res, root, measured_tree);

    return res;
}
