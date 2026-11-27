#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <ranges>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include <kei-utils/result.hh>

#include "sleet/error.hh"
#include "sleet/math/edge_insets.hh"
#include "sleet/math/vec2.hh"
#include "sleet/widget/animation.hh"
#include "sleet/widget/control.hh"
#include "sleet/widget/id.hh"
#include "sleet/widget/style.hh"


namespace sleet::widget
{
    /// The kind of a node. Every node is one of these four primitives, and
    /// larger widgets are built by composing them.
    enum class node_kind : std::uint8_t
    {
        box,
        row,
        column,
        text,
    };


    [[nodiscard]]
    constexpr auto node_kind_to_string(node_kind k) noexcept -> std::string_view
    {
        switch (k)
        {
        case node_kind::box:    return "box";
        case node_kind::row:    return "row";
        case node_kind::column: return "column";
        case node_kind::text:   return "text";
        }

        std::unreachable();
    }


    /// The edge or corner of a parent's content box that a floating node is
    /// positioned against.
    enum class anchor_point : std::uint8_t
    {
        top_left,
        top_center,
        top_right,
        center_left,
        center,
        center_right,
        bottom_left,
        bottom_center,
        bottom_right,
    };


    /// An owning, type-erased widget tree.
    struct node_data
    {
        widget_id   id;
        std::string text;
        std::string action;

        std::vector<node_data> children;
        std::string            name;

        math::vec2_of<sizing> size { sizing::fit(), sizing::fit() };

        math::edge_insets margin;
        math::edge_insets padding;

        std::source_location origin;

        node_data *parent = nullptr;
        font_spec  font;

        /// How text nodes break lines when the content box is narrower than
        /// the text. \c no_wrap keeps one line per paragraph and lets the
        /// renderer clip the overflow.
        text_wrap_mode wrap = text_wrap_mode::wrap;

        /// Overrides \c alignment.x for text. \c std::nullopt falls back to it,
        /// so \c halign keeps working as a shorthand.
        std::optional<text_alignment> text_align;

        /// Enables multiline editing for a text input: Enter inserts a newline,
        /// Up and Down move between wrapped lines, and the runtime keeps the
        /// caret scrolled into view. Ctrl/Cmd+Enter still submits.
        bool multiline = false;

        float gap = 0.F;

        math::vec2_of<align> alignment { align::start, align::start };
        justify              main_axis = justify::start;
        std::optional<align> align_self;

        /// Relative share of available space when this node is \c grow or
        /// \c fill.
        float flex = 1.F;

        node_kind kind = node_kind::box;
        style     visual;
        bool      visible = true;

        /// Eases interactive feedback and explicit style changes over time.
        /// A zero duration keeps the widget instant.
        transition_spec motion;

        /// Floating nodes are taken out of the parent's flow, positioned
        /// against \c placement_anchor inside the parent's content box, and
        /// painted above all in-flow content. \c placement is an additional
        /// offset. Combo-box popups use this to form an overlay.
        bool         floating         = false;
        anchor_point placement_anchor = anchor_point::top_left;
        math::vec2   placement { 0.F, 0.F };

        /// Floating nodes are painted in ascending z-order. In-flow content is
        /// painted first, so any floating node covers it.
        int z_index = 0;

        /// Clips descendants to this node's border box when true. Hit testing
        /// follows the same rule.
        bool clip_children = false;

        /// Allows the runtime to pan descendants with the mouse wheel. Combine
        /// with \c clip_children and a fixed size to build a scroll viewport.
        bool scrollable = false;

        /// Draws a proportional scrollbar for a scrollable node.
        bool show_scrollbar = true;

        /// Scrolls the nearest scrollable ancestors when this node gains focus.
        bool reveal_on_focus = true;

        /// Disabled widgets stay visible but ignore pointer and keyboard input.
        bool disabled = false;

        /// Built-in behaviour that the runtime updates between frames.
        control_kind control = control_kind::none;
        tone         tint    = tone::none;

        /// The visual weight used by button factories.
        button_variant variant = button_variant::standard;

        /// Whether the widget participates in keyboard focus traversal.
        bool focusable = false;

        /// The initial state of an indicator widget.
        bool checked = false;

        /// A normalized value in the range zero to one for sliders.
        float control_value = 0.F;

        /// Hint text shown by an empty text input or combo box.
        std::string placeholder;

        /// Text shown in a floating tooltip after the pointer rests on the
        /// widget or one of its descendants.
        std::string tooltip_text;

        /// Fills the parent's content box. Used by full-window overlays.
        bool backdrop = false;

        /// Limits Tab traversal to this subtree while focus is inside it.
        bool focus_scope = false;

        /// Marks this node as a modal surface. Focus moves into the topmost
        /// visible modal, and Escape dispatches \c cancel_action.
        bool modal = false;

        /// The action dispatched when Escape is pressed inside a modal.
        std::string cancel_action;

        /// Resolves the fill color from the theme instead of an explicit
        /// color. \c tone::none leaves the background unset.
        tone background_tint = tone::none;

        /// Resolves the border color from the theme instead of an explicit
        /// color. \c tone::none uses the control's normal border.
        tone border_tint = tone::none;


        node_data(std::string_view name, std::source_location origin, node_kind kind);
        node_data(const node_data &other);
        node_data(node_data &&other) noexcept;
        node_data &operator=(const node_data &other);
        node_data &operator=(node_data &&other) noexcept;

        ~node_data() = default;


        /// Finds a node by its complete id.
        /// \param key The complete widget id.
        /// \return The matching node.
        /// \throws std::out_of_range if \p key is missing.
        auto operator[](std::string_view key) -> node_data &;
        auto operator[](std::string_view key) const -> const node_data &;

        /// Non-throwing lookup.
        /// \param key The complete widget id.
        /// \return The matching node, or \c nullptr when the id is missing.
        [[nodiscard]] auto find(std::string_view key) noexcept -> node_data *;
        [[nodiscard]] auto find(std::string_view key) const noexcept -> const node_data *;
        [[nodiscard]] auto find(const char *key) noexcept -> node_data *;
        [[nodiscard]] auto find(const char *key) const noexcept -> const node_data *;
        [[nodiscard]] auto find(const widget_id &key) noexcept -> node_data *;
        [[nodiscard]] auto find(const widget_id &key) const noexcept -> const node_data *;


        [[nodiscard]]
        auto children_amount(bool recurse = true) const noexcept -> std::size_t;

        void refresh_parents(bool recurse = true) noexcept;

        /// Walks the tree top-down, sets parent pointers, and fills in empty ids.
        /// Duplicate explicit ids are an error.
        /// \param parent_path The path prefix used for generated ids.
        /// \return An error if two explicit ids collide.
        /// \note Call this once on the root after building the tree.
        auto finalize(std::string_view parent_path = {}) -> kei::result<>;


        [[nodiscard]] auto is_box() const noexcept -> bool { return kind == node_kind::box; }
        [[nodiscard]] auto is_row() const noexcept -> bool { return kind == node_kind::row; }
        [[nodiscard]] auto is_column() const noexcept -> bool { return kind == node_kind::column; }
        [[nodiscard]] auto is_text() const noexcept -> bool { return kind == node_kind::text; }
        [[nodiscard]] auto is_floating() const noexcept -> bool { return floating; }
        [[nodiscard]] auto is_disabled() const noexcept -> bool { return disabled; }

    private:
        auto finalize_impl(std::unordered_set<std::string_view> &seen, std::string_view parent_path)
            -> kei::result<>;
    };


    namespace _impl
    {
        template <typename W>
        concept widget = requires(std::remove_cvref_t<W> w) {
            { std::move(w).release() } -> std::same_as<node_data>;
            { std::remove_cvref_t<W>::name } -> std::convertible_to<std::string_view>;
            { std::remove_cvref_t<W>::kind } -> std::convertible_to<node_kind>;
            { std::as_const(w).data_view() } -> std::same_as<const node_data &>;
        };


        template <typename W>
        concept widget_like = widget<W> or std::same_as<std::remove_cvref_t<W>, node_data>;


        /// Copies from lvalues, moves from rvalues.
        template <widget W>
        auto take_data(W &&widget) -> node_data
        {
            if constexpr (std::is_lvalue_reference_v<W>)
                return widget.data_view();
            else
                return std::forward<W>(widget).release();
        }

        /// An already type-erased widget is passed through.
        inline auto take_data(node_data data) -> node_data { return data; }


        template <std::invocable<node_data &> F>
        struct modifier_fn
        {
            F fn;


            modifier_fn(F &&func) : fn { std::move(func) } {}
            modifier_fn(const F &func) : fn { func } {}

            void apply(node_data &data) const { fn(data); }
        };


        template <typename R>
        concept widget_range
            = std::ranges::input_range<R> and widget_like<std::ranges::range_value_t<R>>;

        /// A function that takes no arguments and produces one widget or a range
        /// of widgets.
        template <typename F>
        concept child_factory
            = std::invocable<F>
          and (widget_like<std::invoke_result_t<F>> or widget_range<std::invoke_result_t<F>>);
    }


    /// Converts any widget into its type-erased representation.
    template <_impl::widget W>
    [[nodiscard]]
    auto to_data(W &&widget) -> node_data
    { return _impl::take_data(std::forward<W>(widget)); }


    /// An already type-erased widget is passed through.
    [[nodiscard]]
    inline auto to_data(node_data data) -> node_data
    { return data; }


    template <node_kind Kind>
    class node
    {
    public:
        static constexpr auto name = node_kind_to_string(Kind);
        static constexpr auto kind = Kind;


        node(std::source_location origin) noexcept
            requires(kind != node_kind::text)
            : m_data { name, origin, kind }
        {
        }

        node(std::string text, std::source_location origin)
            requires(kind == node_kind::text)
            : m_data { name, origin, kind }
        { m_data.text = std::move(text); }


        [[nodiscard]]
        auto finalize(std::string_view parent_path = {}) -> kei::result<>
        { return m_data.finalize(parent_path); }


        [[nodiscard]]
        auto data_view() const noexcept -> const node_data &
        { return m_data; }


        [[nodiscard]]
        auto release() && noexcept -> node_data
        { return std::move(m_data); }


        template <typename Self, typename F>
        auto operator|(this Self &&self, const _impl::modifier_fn<F> &modifier)
            -> std::remove_cvref_t<Self>
        {
            auto result = prepare(std::forward<Self>(self));
            modifier.apply(result.m_data);
            return result;
        }

        template <typename F>
        auto operator|=(const _impl::modifier_fn<F> &modifier) -> node &
        {
            modifier.apply(m_data);
            return *this;
        }

        template <typename Self, _impl::widget_like... W>
        auto operator[](this Self &&self, W &&...kids) -> std::remove_cvref_t<Self>
            requires(kind != node_kind::text)
        {
            auto result = prepare(std::forward<Self>(self));
            result.m_data.children.reserve(result.m_data.children.size() + sizeof...(W));
            (result.m_data.children.emplace_back(_impl::take_data(std::forward<W>(kids))), ...);
            return result;
        }


        template <typename Self, _impl::child_factory F>
        auto operator[](this Self &&self, F &&factory) -> std::remove_cvref_t<Self>
            requires(kind != node_kind::text)
        {
            auto result    = prepare(std::forward<Self>(self));
            using result_t = std::invoke_result_t<F>;

            if constexpr (_impl::widget_like<result_t>)
                result.m_data.children.emplace_back(
                    _impl::take_data(std::invoke(std::forward<F>(factory))));
            else
            {
                auto &&range = std::invoke(std::forward<F>(factory));
                if constexpr (std::ranges::sized_range<std::remove_cvref_t<result_t>>)
                    result.m_data.children.reserve(
                        result.m_data.children.size()
                        + static_cast<std::size_t>(std::ranges::size(range)));

                for (auto &&kid : range)
                    result.m_data.children.emplace_back(
                        _impl::take_data(std::forward<decltype(kid)>(kid)));
            }

            return result;
        }

    private:
        node_data m_data;


    protected:
        [[nodiscard]] auto data() noexcept -> node_data & { return m_data; }


    private:
        template <typename Self>
        static auto prepare(Self &&self) -> std::remove_cvref_t<Self>
        { return std::forward<Self>(self); }
    };


    /// Resolves the alignment used to paint text. An explicit \c text_align
    /// wins; otherwise the node's horizontal layout alignment is used.
    /// \param node The text-bearing node.
    /// \return The effective text alignment.
    [[nodiscard]]
    auto text_alignment_of(const node_data &node) noexcept -> text_alignment;

    /// Returns the horizontal offset of a line of text inside \p free_space.
    /// \param alignment The line's text alignment.
    /// \param free_space The unused width beside the line.
    /// \return The left inset for the line.
    [[nodiscard]]
    auto text_alignment_offset(text_alignment alignment, float free_space) noexcept -> float;
}
