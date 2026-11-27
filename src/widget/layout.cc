#include <algorithm>
#include <cmath>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

#include "sleet/widget/layout.hh"


namespace
{
    using sleet::math::edge_insets;
    using sleet::math::rect;
    using sleet::math::vec2;
    using sleet::widget::align;
    using sleet::widget::anchor_point;
    using sleet::widget::font_spec;
    using sleet::widget::justify;
    using sleet::widget::node_data;
    using sleet::widget::sizing;
    using sleet::widget::text_measurer;


    enum class axis : std::uint8_t
    {
        width  = 0,
        height = 1,
    };


    [[nodiscard]]
    auto clamp0(float value) noexcept -> float
    {
        if (!std::isfinite(value)) return 0.F;
        return value < 0.F ? 0.F : value;
    }

    [[nodiscard]]
    constexpr auto other(axis a) noexcept -> axis
    { return a == axis::width ? axis::height : axis::width; }

    [[nodiscard]]
    constexpr auto size_axis(vec2 value, axis a) noexcept -> float
    { return a == axis::width ? value.x : value.y; }


    constexpr void set_axis(vec2 &value, axis a, float amount) noexcept
    {
        if (a == axis::width)
            value.x = amount;
        else
            value.y = amount;
    }


    [[nodiscard]]
    constexpr auto margin_axis(const edge_insets &m, axis a) noexcept -> float
    { return a == axis::width ? m.horizontal() : m.vertical(); }

    [[nodiscard]]
    constexpr auto padding_axis(const edge_insets &p, axis a) noexcept -> float
    { return a == axis::width ? p.horizontal() : p.vertical(); }

    [[nodiscard]]
    constexpr auto leading_margin(const edge_insets &m, axis a) noexcept -> float
    { return a == axis::width ? m.left : m.top; }

    [[nodiscard]]
    constexpr auto trailing_margin(const edge_insets &m, axis a) noexcept -> float
    { return a == axis::width ? m.right : m.bottom; }

    [[nodiscard]]
    constexpr auto is_stacked(const node_data &node, axis a) noexcept -> bool
    { return (node.is_row() and a == axis::width) or (node.is_column() and a == axis::height); }


    /// Floating children are measured but do not participate in their parent's
    /// intrinsic size or flow.
    [[nodiscard]]
    constexpr auto in_flow(const node_data &node) noexcept -> bool
    { return node.visible and !node.floating; }

    [[nodiscard]]
    auto align_offset(align alignment, float free_space) noexcept -> float
    {
        const auto free = std::max(0.F, free_space);

        switch (alignment)
        {
        case align::start:  return 0.F;
        case align::center: return free / 2.F;
        case align::end:    return free;
        }

        return 0.F;
    }


    /// Resolves an anchor inside a content box. Margins inset the child from
    /// the anchored edge; centered axes ignore margins so the child stays
    /// centered even when it has asymmetric spacing.
    [[nodiscard]]
    auto anchor_position(anchor_point       anchor,
                         vec2               content_position,
                         vec2               content_size,
                         vec2               child_size,
                         const edge_insets &margin) noexcept -> vec2
    {
        const auto left     = content_position.x + margin.left;
        const auto center_x = content_position.x + ((content_size.x - child_size.x) / 2.F);
        const auto right    = content_position.x + content_size.x - child_size.x - margin.right;

        const auto top      = content_position.y + margin.top;
        const auto center_y = content_position.y + ((content_size.y - child_size.y) / 2.F);
        const auto bottom   = content_position.y + content_size.y - child_size.y - margin.bottom;

        switch (anchor)
        {
        case anchor_point::top_left:      return { left, top };
        case anchor_point::top_center:    return { center_x, top };
        case anchor_point::top_right:     return { right, top };
        case anchor_point::center_left:   return { left, center_y };
        case anchor_point::center:        return { center_x, center_y };
        case anchor_point::center_right:  return { right, center_y };
        case anchor_point::bottom_left:   return { left, bottom };
        case anchor_point::bottom_center: return { center_x, bottom };
        case anchor_point::bottom_right:  return { right, bottom };
        }

        return content_position;
    }

    struct measured
    {
        const node_data      *widget = nullptr;
        std::vector<measured> children;

        vec2 intrinsic { 0.F, 0.F };
        vec2 size { 0.F, 0.F };
        vec2 position { 0.F, 0.F };
    };


    /// Moves an arranged subtree without measuring it again.
    void translate(measured &node, vec2 delta) noexcept
    {
        node.position += delta;
        for (auto &child : node.children) translate(child, delta);
    }


    [[nodiscard]]
    auto build_measured(const node_data &widget) -> measured
    {
        measured result;
        result.widget = &widget;
        result.children.reserve(widget.children.size());

        for (const auto &child : widget.children)
            result.children.emplace_back(build_measured(child));

        return result;
    }


    [[nodiscard]]
    auto effective_width_limit(const node_data &node, std::optional<float> width_limit)
        -> std::optional<float>
    {
        if (node.wrap == sleet::widget::text_wrap_mode::no_wrap) return std::nullopt;
        return width_limit;
    }


    [[nodiscard]]
    auto measure_text_node(const node_data     &node,
                           const text_measurer &measure,
                           std::optional<float> width_limit) -> kei::result<vec2>
    {
        auto content
            = measure(node.text, node.font, effective_width_limit(node, width_limit), node.wrap);
        if (!content) return std::move(content.error()).unexpected();

        vec2 outer {
            content->x + node.padding.horizontal(),
            content->y + node.padding.vertical(),
        };

        if (node.size.x.is_fixed()) outer.x = node.size.x.get_fixed_size();
        if (node.size.y.is_fixed()) outer.y = node.size.y.get_fixed_size();

        return vec2 { clamp0(outer.x), clamp0(outer.y) };
    }


    [[nodiscard]]
    auto measure_intrinsic(const node_data &node, measured &result, const text_measurer &measure)
        -> kei::result<>
    {
        result.widget = &node;

        if (!node.visible)
        {
            result.children.clear();
            result.intrinsic = { 0.F, 0.F };
            return {};
        }

        result.children.resize(node.children.size());

        for (std::size_t i = 0; i < node.children.size(); i++)
            if (auto res = measure_intrinsic(node.children[i], result.children[i], measure); !res)
                return res;

        if (node.is_text())
        {
            std::optional<float> limit;
            if (node.size.x.is_fixed())
                limit = clamp0(node.size.x.get_fixed_size() - node.padding.horizontal());

            auto size = measure_text_node(node, measure, limit);
            if (!size) return std::move(size.error()).unexpected();

            result.intrinsic = *size;
            return {};
        }

        for (const auto a : { axis::width, axis::height })
        {
            float content = 0.F;

            std::size_t visible_count = 0;

            for (std::size_t i = 0; i < node.children.size(); i++)
            {
                const auto &child = node.children[i];
                if (!in_flow(child)) continue;
                visible_count++;

                if (is_stacked(node, a))
                    content += result.children[i].intrinsic[static_cast<std::size_t>(a)]
                             + margin_axis(child.margin, a);
                else
                    content = std::max(content,
                                       result.children[i].intrinsic[static_cast<std::size_t>(a)]
                                           + margin_axis(child.margin, a));
            }

            if (is_stacked(node, a) and visible_count > 1)
                content += std::max(0.F, node.gap) * static_cast<float>(visible_count - 1);

            content += padding_axis(node.padding, a);

            if (node.size[static_cast<std::size_t>(a)].is_fixed())
                content = node.size[static_cast<std::size_t>(a)].get_fixed_size();

            result.intrinsic[static_cast<std::size_t>(a)] = clamp0(content);
        }

        return {};
    }


    [[nodiscard]]
    auto remeasure_text(const node_data     &node,
                        vec2                &size,
                        const text_measurer &measure,
                        std::optional<float> available_width) -> kei::result<>
    {
        if (!node.is_text()) return {};

        const bool width_fit  = node.size.x.is_fit();
        const bool height_fit = node.size.y.is_fit();
        if (!width_fit and !height_fit) return {};

        std::optional<float> limit;
        if (!width_fit)
            limit = clamp0(size.x - node.padding.horizontal());
        else if (available_width)
            limit = clamp0(*available_width - node.padding.horizontal());

        auto measured_size
            = measure(node.text, node.font, effective_width_limit(node, limit), node.wrap);
        if (!measured_size) return std::move(measured_size.error()).unexpected();

        if (width_fit) size.x = clamp0(measured_size->x + node.padding.horizontal());
        if (height_fit) size.y = clamp0(measured_size->y + node.padding.vertical());

        return {};
    }


    [[nodiscard]]
    auto arrange(const node_data     &node,
                 measured            &result,
                 vec2                 position,
                 vec2                 size,
                 const text_measurer &measure) -> kei::result<>;

    class stack_arrangement
    {
    public:
        stack_arrangement(const node_data     &node,
                          measured            &result,
                          vec2                 content_position,
                          vec2                 content_size,
                          const text_measurer &measure)
            : m_node { node }, m_result { result }, m_content_position { content_position },
              m_content_size { content_size }, m_measure { measure },
              m_main { node.is_row() ? axis::width : axis::height }, m_cross { other(m_main) },
              m_count { node.children.size() },
              m_main_fit { node.size[static_cast<std::size_t>(m_main)].is_fit()
                           and !node.backdrop },
              m_cross_fit { node.size[static_cast<std::size_t>(m_cross)].is_fit()
                            and !node.backdrop }
        {
        }


        auto run() -> kei::result<>
        {
            collect_sizes();
            if (auto res = remeasure_cross_axis(); !res) return res;
            distribute_main_space();
            if (auto res = remeasure_main_axis(); !res) return res;
            update_fit_axes();
            if (auto res = place_children(false); !res) return res;
            update_resolved_axes();
            return place_children(true);
        }


    private:
        const node_data     &m_node;
        measured            &m_result;
        vec2                 m_content_position;
        vec2                 m_content_size;
        const text_measurer &m_measure;
        axis                 m_main;
        axis                 m_cross;
        std::size_t          m_count;
        bool                 m_main_fit;
        bool                 m_cross_fit;
        std::size_t          m_visible_count = 0;
        float                m_gap_total     = 0.F;
        std::vector<vec2>    m_sizes;
        std::vector<float>   m_weights;


        void collect_sizes()
        {
            m_sizes.resize(m_count);
            m_weights.assign(m_count, 0.F);

            for (std::size_t i = 0; i < m_count; i++)
            {
                const auto &child = m_node.children[i];
                m_sizes[i]        = m_result.children[i].intrinsic;

                if (!in_flow(child)) continue;
                m_visible_count++;

                const auto main_size = child.size[static_cast<std::size_t>(m_main)];
                if (main_size.is_fixed())
                    m_sizes[i][static_cast<std::size_t>(m_main)] = main_size.get_fixed_size();

                const auto cross_size = child.size[static_cast<std::size_t>(m_cross)];
                if (cross_size.is_fixed())
                    m_sizes[i][static_cast<std::size_t>(m_cross)] = cross_size.get_fixed_size();
                else if (!m_cross_fit and cross_size.is_fill())
                    m_sizes[i][static_cast<std::size_t>(m_cross)] = clamp0(
                        size_axis(m_content_size, m_cross) - margin_axis(child.margin, m_cross));

                if (!m_main_fit and (main_size.is_grow() or main_size.is_fill()))
                    m_weights[i] = std::max(0.F, child.flex);
            }

            m_gap_total = m_visible_count > 1
                            ? std::max(0.F, m_node.gap) * static_cast<float>(m_visible_count - 1)
                            : 0.F;
        }


        auto remeasure_cross_axis() -> kei::result<>
        {
            if (!m_node.is_column()) return {};

            for (std::size_t i = 0; i < m_count; i++)
            {
                const auto &child = m_node.children[i];
                if (!in_flow(child)) continue;

                const auto available_width
                    = m_cross_fit
                        ? std::optional<float> {}
                        : std::optional<float> { clamp0(size_axis(m_content_size, m_cross)
                                                        - margin_axis(child.margin, m_cross)) };

                if (auto res = remeasure_text(child, m_sizes[i], m_measure, available_width); !res)
                    return res;
            }

            return {};
        }


        [[nodiscard]]
        auto occupied() const noexcept -> float
        {
            float result = m_gap_total;
            for (std::size_t i = 0; i < m_count; i++)
                if (in_flow(m_node.children[i]))
                    result += size_axis(m_sizes[i], m_main)
                            + margin_axis(m_node.children[i].margin, m_main);
            return result;
        }


        void distribute_main_space()
        {
            if (m_main_fit) return;

            const auto free_space = size_axis(m_content_size, m_main) - occupied();

            float weight_sum = 0.F;
            for (const auto weight : m_weights) weight_sum += weight;

            if (free_space > 0.F and weight_sum > 0.F)
            {
                for (std::size_t i = 0; i < m_count; i++)
                    if (m_weights[i] > 0.F)
                        m_sizes[i][static_cast<std::size_t>(m_main)]
                            += free_space * m_weights[i] / weight_sum;
                return;
            }

            if (free_space >= 0.F) return;

            float reducible = 0.F;
            for (std::size_t i = 0; i < m_count; i++)
                if (m_weights[i] > 0.F) reducible += m_sizes[i][static_cast<std::size_t>(m_main)];

            if (reducible <= 0.F) return;

            const auto factor = clamp0(1.F + (free_space / reducible));
            for (std::size_t i = 0; i < m_count; i++)
                if (m_weights[i] > 0.F) m_sizes[i][static_cast<std::size_t>(m_main)] *= factor;
        }


        auto remeasure_main_axis() -> kei::result<>
        {
            if (!m_node.is_row()) return {};

            for (std::size_t i = 0; i < m_count; i++)
                if (in_flow(m_node.children[i]))
                    if (auto res
                        = remeasure_text(m_node.children[i], m_sizes[i], m_measure, std::nullopt);
                        !res)
                        return res;

            return {};
        }


        void update_fit_axes()
        {
            if (m_main_fit)
            {
                float content = m_gap_total;
                for (std::size_t i = 0; i < m_count; i++)
                    if (in_flow(m_node.children[i]))
                        content += m_sizes[i][static_cast<std::size_t>(m_main)]
                                 + margin_axis(m_node.children[i].margin, m_main);

                m_content_size[static_cast<std::size_t>(m_main)] = content;
                m_result.size[static_cast<std::size_t>(m_main)]
                    = clamp0(content + padding_axis(m_node.padding, m_main));
            }

            if (!m_cross_fit) return;

            float content = 0.F;
            for (std::size_t i = 0; i < m_count; i++)
                if (in_flow(m_node.children[i]))
                    content
                        = std::max(content, m_sizes[i][static_cast<std::size_t>(m_cross)]
                                                + margin_axis(m_node.children[i].margin, m_cross));

            m_content_size[static_cast<std::size_t>(m_cross)] = content;
            m_result.size[static_cast<std::size_t>(m_cross)]
                = clamp0(content + padding_axis(m_node.padding, m_cross));
        }


        [[nodiscard]]
        auto justify_offsets(float occupied_size) const noexcept -> std::pair<float, float>
        {
            const auto free_space
                = m_main_fit ? 0.F : size_axis(m_content_size, m_main) - occupied_size;

            switch (m_node.main_axis)
            {
            case justify::start:  return { 0.F, 0.F };
            case justify::center: return { std::max(0.F, free_space) / 2.F, 0.F };
            case justify::end:    return { std::max(0.F, free_space), 0.F };
            case justify::between:
                if (m_visible_count > 1)
                    return { 0.F,
                             std::max(0.F, free_space) / static_cast<float>(m_visible_count - 1) };
                return { 0.F, 0.F };
            }

            return { 0.F, 0.F };
        }


        [[nodiscard]]
        auto child_position(std::size_t index, vec2 child_size, float cursor) const noexcept -> vec2
        {
            const auto &child       = m_node.children[index];
            const auto  cross_space = size_axis(m_content_size, m_cross)
                                    - size_axis(child_size, m_cross)
                                    - margin_axis(child.margin, m_cross);
            const auto  cross_align = child.align_self.value_or(
                m_cross == axis::width ? m_node.alignment.x : m_node.alignment.y);

            auto position = vec2 {};
            set_axis(position, m_main, cursor + leading_margin(child.margin, m_main));
            set_axis(position, m_cross,
                     size_axis(m_content_position, m_cross) + leading_margin(child.margin, m_cross)
                         + align_offset(cross_align, cross_space));
            return position;
        }


        auto place_children(bool final_pass) -> kei::result<>
        {
            float occupied_size = 0.F;
            if (final_pass)
            {
                occupied_size = m_gap_total;
                for (std::size_t i = 0; i < m_count; i++)
                    if (in_flow(m_node.children[i]))
                        occupied_size += m_result.children[i].size[static_cast<std::size_t>(m_main)]
                                       + margin_axis(m_node.children[i].margin, m_main);
            }
            else
                occupied_size = occupied();

            const auto [offset, extra_gap] = justify_offsets(occupied_size);
            float cursor                   = size_axis(m_content_position, m_main) + offset;

            for (std::size_t i = 0; i < m_count; i++)
            {
                const auto &child = m_node.children[i];
                if (!in_flow(child)) continue;

                const auto size     = final_pass ? m_result.children[i].size : m_sizes[i];
                const auto position = child_position(i, size, cursor);

                if (final_pass)
                    translate(m_result.children[i], position - m_result.children[i].position);
                else if (auto res
                         = arrange(child, m_result.children[i], position, m_sizes[i], m_measure);
                         !res)
                    return res;

                cursor += leading_margin(child.margin, m_main) + size_axis(size, m_main)
                        + trailing_margin(child.margin, m_main) + std::max(0.F, m_node.gap)
                        + extra_gap;
            }

            return {};
        }


        void update_resolved_axes()
        {
            if (!m_main_fit and !m_cross_fit) return;

            float final_main  = m_gap_total;
            float final_cross = 0.F;

            for (std::size_t i = 0; i < m_count; i++)
                if (in_flow(m_node.children[i]))
                {
                    final_main += m_result.children[i].size[static_cast<std::size_t>(m_main)]
                                + margin_axis(m_node.children[i].margin, m_main);
                    final_cross = std::max(
                        final_cross, m_result.children[i].size[static_cast<std::size_t>(m_cross)]
                                         + margin_axis(m_node.children[i].margin, m_cross));
                }

            if (m_main_fit)
            {
                m_content_size[static_cast<std::size_t>(m_main)] = final_main;
                m_result.size[static_cast<std::size_t>(m_main)]
                    = clamp0(final_main + padding_axis(m_node.padding, m_main));
            }

            if (m_cross_fit)
            {
                m_content_size[static_cast<std::size_t>(m_cross)] = final_cross;
                m_result.size[static_cast<std::size_t>(m_cross)]
                    = clamp0(final_cross + padding_axis(m_node.padding, m_cross));
            }
        }
    };


    [[nodiscard]]
    auto arrange_row_or_column(const node_data     &node,
                               measured            &result,
                               vec2                 content_position,
                               vec2                 content_size,
                               const text_measurer &measure) -> kei::result<>
    { return stack_arrangement { node, result, content_position, content_size, measure }.run(); }


    class box_arrangement
    {
    public:
        box_arrangement(const node_data     &node,
                        measured            &result,
                        vec2                 content_position,
                        vec2                 content_size,
                        const text_measurer &measure)
            : m_node { node }, m_result { result }, m_content_position { content_position },
              m_content_size { content_size }, m_measure { measure },
              m_count { node.children.size() },
              m_width_fit { node.size.x.is_fit() and !node.backdrop },
              m_height_fit { node.size.y.is_fit() and !node.backdrop }
        {
        }


        auto run() -> kei::result<>
        {
            if (auto res = collect_sizes(); !res) return res;
            update_fit_axes();
            if (auto res = place_children(false); !res) return res;
            update_resolved_axes();
            return place_children(true);
        }


    private:
        const node_data     &m_node;
        measured            &m_result;
        vec2                 m_content_position;
        vec2                 m_content_size;
        const text_measurer &m_measure;
        std::size_t          m_count;
        bool                 m_width_fit;
        bool                 m_height_fit;
        std::vector<vec2>    m_sizes;


        auto collect_sizes() -> kei::result<>
        {
            m_sizes.resize(m_count);

            for (std::size_t i = 0; i < m_count; i++)
            {
                const auto &child = m_node.children[i];
                if (!in_flow(child)) continue;

                vec2 size = m_result.children[i].intrinsic;

                for (const auto a : { axis::width, axis::height })
                {
                    const auto child_size = child.size[static_cast<std::size_t>(a)];
                    if (child_size.is_fixed())
                        size[static_cast<std::size_t>(a)] = child_size.get_fixed_size();
                    else if (!m_node.size[static_cast<std::size_t>(a)].is_fit()
                             and (child_size.is_fill() or child_size.is_grow()))
                        size[static_cast<std::size_t>(a)]
                            = clamp0(size_axis(m_content_size, a) - margin_axis(child.margin, a));
                }

                const auto available_width
                    = m_width_fit
                        ? std::optional<float> {}
                        : std::optional<float> { clamp0(m_content_size.x
                                                        - margin_axis(child.margin, axis::width)) };

                if (auto res = remeasure_text(child, size, m_measure, available_width); !res)
                    return res;

                m_sizes[i] = size;
            }

            return {};
        }


        void update_fit_axes()
        {
            if (m_width_fit)
            {
                float content = 0.F;
                for (std::size_t i = 0; i < m_count; i++)
                    if (in_flow(m_node.children[i]))
                        content = std::max(
                            content,
                            m_sizes[i].x + margin_axis(m_node.children[i].margin, axis::width));

                m_content_size.x = content;
                m_result.size.x  = clamp0(content + m_node.padding.horizontal());
            }

            if (!m_height_fit) return;

            float content = 0.F;
            for (std::size_t i = 0; i < m_count; i++)
                if (in_flow(m_node.children[i]))
                    content = std::max(content,
                                       m_sizes[i].y
                                           + margin_axis(m_node.children[i].margin, axis::height));

            m_content_size.y = content;
            m_result.size.y  = clamp0(content + m_node.padding.vertical());
        }


        [[nodiscard]]
        auto child_position(std::size_t index, vec2 child_size) const noexcept -> vec2
        {
            const auto &child = m_node.children[index];
            const auto  x_space
                = m_content_size.x - child_size.x - margin_axis(child.margin, axis::width);
            const auto y_space
                = m_content_size.y - child_size.y - margin_axis(child.margin, axis::height);

            const auto horizontal = child.align_self.value_or(m_node.alignment.x);
            const auto vertical   = child.align_self.value_or(m_node.alignment.y);

            return {
                m_content_position.x + child.margin.left + align_offset(horizontal, x_space),
                m_content_position.y + child.margin.top + align_offset(vertical, y_space),
            };
        }


        auto place_children(bool final_pass) -> kei::result<>
        {
            for (std::size_t i = 0; i < m_count; i++)
            {
                const auto &child = m_node.children[i];
                if (!in_flow(child)) continue;

                const auto size     = final_pass ? m_result.children[i].size : m_sizes[i];
                const auto position = child_position(i, size);

                if (final_pass)
                    translate(m_result.children[i], position - m_result.children[i].position);
                else if (auto res
                         = arrange(child, m_result.children[i], position, m_sizes[i], m_measure);
                         !res)
                    return res;
            }

            return {};
        }


        void update_resolved_axes()
        {
            if (!m_width_fit and !m_height_fit) return;

            float final_width  = 0.F;
            float final_height = 0.F;

            for (std::size_t i = 0; i < m_count; i++)
                if (in_flow(m_node.children[i]))
                {
                    final_width = std::max(
                        final_width, m_result.children[i].size.x
                                         + margin_axis(m_node.children[i].margin, axis::width));
                    final_height = std::max(
                        final_height, m_result.children[i].size.y
                                          + margin_axis(m_node.children[i].margin, axis::height));
                }

            if (m_width_fit)
            {
                m_content_size.x = final_width;
                m_result.size.x  = clamp0(final_width + m_node.padding.horizontal());
            }

            if (m_height_fit)
            {
                m_content_size.y = final_height;
                m_result.size.y  = clamp0(final_height + m_node.padding.vertical());
            }
        }
    };


    [[nodiscard]]
    auto arrange_box(const node_data     &node,
                     measured            &result,
                     vec2                 content_position,
                     vec2                 content_size,
                     const text_measurer &measure) -> kei::result<>
    { return box_arrangement { node, result, content_position, content_size, measure }.run(); }


    /// Positions floating children after the in-flow content has a resolved
    /// size. Floating children use their intrinsic height unless it is fixed;
    /// fill and grow only stretch the horizontal axis so a popup does not
    /// become as tall as its parent. The anchor resolves against the parent's
    /// content box, then \c placement adds a local offset.
    [[nodiscard]]
    auto arrange_floating_children(const node_data     &node,
                                   measured            &result,
                                   vec2                 content_position,
                                   vec2                 content_size,
                                   const text_measurer &measure) -> kei::result<>
    {
        for (std::size_t i = 0; i < node.children.size(); i++)
        {
            const auto &child = node.children[i];
            if (!child.visible or !child.floating) continue;

            auto size = result.children[i].intrinsic;

            if (child.backdrop) { size = content_size; }
            else
            {
                if (child.size.x.is_fixed())
                    size.x = child.size.x.get_fixed_size();
                else if (child.size.x.is_fill() or child.size.x.is_grow())
                    size.x = clamp0(content_size.x - child.margin.horizontal());

                if (child.size.y.is_fixed()) size.y = child.size.y.get_fixed_size();
            }

            // A floating text node that fills its parent still needs to wrap at
            // the resolved width, just like an in-flow text child.
            if (auto res = remeasure_text(child, size, measure, std::optional<float> { size.x });
                !res)
                return res;

            const auto position = child.backdrop
                                    ? content_position
                                    : anchor_position(child.placement_anchor, content_position,
                                                      content_size, size, child.margin)
                                          + child.placement;

            if (auto res = arrange(child, result.children[i], position, size, measure); !res)
                return res;
        }

        return {};
    }

    auto arrange(const node_data     &node,
                 measured            &result,
                 vec2                 position,
                 vec2                 size,
                 const text_measurer &measure) -> kei::result<>
    {
        result.position = position;
        result.size     = size;

        if (!node.visible) return {};

        if (node.is_text())
        {
            // Keep any width imposed by the parent. remeasure_text uses it as
            // the wrap limit for fit-width text and ignores it for fill or fixed
            // sizing.
            if (auto res = remeasure_text(node, result.size, measure,
                                          std::optional<float> { clamp0(result.size.x) });
                !res)
                return res;
            return {};
        }

        if (node.children.empty()) return {};

        const auto content_position
            = vec2 { position.x + node.padding.left, position.y + node.padding.top };
        const auto content_size = vec2 {
            clamp0(size.x - node.padding.horizontal()),
            clamp0(size.y - node.padding.vertical()),
        };

        if (node.is_row() or node.is_column())
        {
            if (auto res
                = arrange_row_or_column(node, result, content_position, content_size, measure);
                !res)
                return res;
        }
        else if (auto res = arrange_box(node, result, content_position, content_size, measure);
                 !res)
            return res;

        return arrange_floating_children(node, result, content_position, content_size, measure);
    }


    [[nodiscard]]
    auto collect_rects(const node_data              &node,
                       const measured               &result,
                       sleet::widget::layout_result &out) -> kei::result<>
    {
        if (!node.visible) return {};

        if (!out.rects.try_emplace(node.id, rect { result.position, result.size }).second)
            return sleet::error { "Duplicate widget id \"{}\".", node.id.view() }.unexpected();

        if (node.floating and node.parent != nullptr)
            out.floating.push_back({ node.id, node.z_index });

        for (std::size_t i = 0; i < node.children.size(); i++)
            if (auto res = collect_rects(node.children[i], result.children[i], out); !res)
                return res;

        return {};
    }
}


auto sleet::widget::calculate_layout(const node_data     &root,
                                     const text_measurer &measure,
                                     math::vec2           viewport) -> kei::result<layout_result>
{
    if (!measure) return sleet::error { "calculate_layout requires a text measurer." }.unexpected();

    auto measured_tree = build_measured(root);

    if (auto res = measure_intrinsic(root, measured_tree, measure); !res)
        return std::move(res.error()).unexpected();

    vec2 root_size = measured_tree.intrinsic;
    for (const auto a : { axis::width, axis::height })
    {
        if (root.size[static_cast<std::size_t>(a)].is_fixed())
            root_size[static_cast<std::size_t>(a)]
                = root.size[static_cast<std::size_t>(a)].get_fixed_size();
        else if (root.size[static_cast<std::size_t>(a)].is_grow()
                 or root.size[static_cast<std::size_t>(a)].is_fill())
            root_size[static_cast<std::size_t>(a)] = clamp0(viewport[static_cast<std::size_t>(a)]);
    }

    if (auto res = arrange(root, measured_tree, { 0.F, 0.F }, root_size, measure); !res)
        return std::move(res.error()).unexpected();

    layout_result out;
    out.root = { measured_tree.position, measured_tree.size };
    out.rects.reserve(root.children_amount() + 1);

    if (auto res = collect_rects(root, measured_tree, out); !res)
        return std::move(res.error()).unexpected();

    std::stable_sort(out.floating.begin(), out.floating.end(),
                     [](const floating_entry &first, const floating_entry &second)
                     { return first.z_index < second.z_index; });

    return out;
}


auto sleet::widget::calculate_layout(const node_data           &root,
                                     const basic_text_measurer &measure_text,
                                     math::vec2 viewport) -> kei::result<layout_result>
{
    if (!measure_text)
        return sleet::error { "calculate_layout requires a text measurer." }.unexpected();

    text_measurer adapted = [measure_text](std::string_view text, const font_spec &font,
                                           std::optional<float>          width_limit,
                                           sleet::widget::text_wrap_mode wrap) -> kei::result<vec2>
    {
        if (wrap == sleet::widget::text_wrap_mode::no_wrap) width_limit.reset();
        return measure_text(text, font, width_limit);
    };

    return calculate_layout(root, adapted, viewport);
}
