#pragma once
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include <kei-utils/bitmask.hh>
#include <kei-utils/result.hh>

#include "sleet/math/vec2.hh"
#include "sleet/math/vec4.hh"
#include "sleet/widget/id.hh"
#include "sleet/widget/style.hh"


namespace sleet::widget
{
    namespace _impl
    {
        template <std::size_t N>
        struct widget_name
        {
            std::array<char, N> string;


            // NOLINTNEXTLINE(modernize-avoid-c-arrays, cppcoreguidelines-avoid-c-arrays)
            constexpr widget_name(const char (&name)[N]) { std::copy_n(name, N, string.begin()); }


            [[nodiscard]] auto view() const && -> std::string_view = delete;
            [[nodiscard]]
            constexpr auto view() const & -> std::string_view
            { return { string.data(), N - 1 }; }
        };
    }


    /// The "type" of a node. A node's basically always either these types (box, row, column,
    /// or text), and any complex widgets structures can be simplified down to these four types.
    enum class node_kind : std::uint8_t
    {
        box,
        row,
        column,
        text,
    };

    static constexpr auto node_kind_to_string(node_kind k) noexcept -> std::string_view
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

    template <node_kind Kind>
    static constexpr auto node_kind_to_string() noexcept
    {
        if constexpr (Kind == node_kind::box) return _impl::widget_name { "box" };
        if constexpr (Kind == node_kind::row) return _impl::widget_name { "row" };
        if constexpr (Kind == node_kind::column) return _impl::widget_name { "column" };
        if constexpr (Kind == node_kind::text) return _impl::widget_name { "text" };

        std::unreachable();
    }


    namespace _impl
    {
        struct node_data
        {
            widget_id   id;
            std::string text;

            std::vector<node_data> children;
            std::string_view       name;

            math::tvec2<sizing> size { sizing::fit(), sizing::fit() };

            math::vec4 margin { 0, 0, 0, 0 };
            math::vec4 padding { 0, 0, 0, 0 };

            std::source_location origin;

            node_data *parent = nullptr;
            font_spec  font;

            float gap = 0.F;

            math::tvec2<align> alignment { align::start, align::start };

            node_kind kind;


            node_data(std::string_view name, std::source_location origin, node_kind kind) noexcept;
            node_data(node_data &&other) noexcept;
            node_data(const node_data &other);

            auto operator=(node_data &&other) noexcept -> node_data & = default;
            auto operator=(const node_data &other) -> node_data &     = default;


            [[nodiscard]]
            auto children_amount(bool recurse = true) const noexcept -> std::size_t;
            void refresh_parents(bool recurse = true) noexcept;

            /// Sets parent pointers and fills in unset ids, top-down.
            /// Call once on the root; the tree must not be moved afterwards.
            void finalize(std::string_view parent_path = {}) noexcept;


            [[nodiscard]] auto is_box() const noexcept -> bool { return kind == node_kind::box; }
            [[nodiscard]] auto is_row() const noexcept -> bool { return kind == node_kind::row; }
            [[nodiscard]] auto is_text() const noexcept -> bool { return kind == node_kind::text; }
            [[nodiscard]] auto is_column() const noexcept -> bool
            { return kind == node_kind::column; }
        };


        template <typename W>
        concept widget = requires(std::remove_cvref_t<W> w) {
            { std::move(w).release() } noexcept -> std::same_as<node_data>;
            { std::remove_cvref_t<W>::name } -> std::convertible_to<std::string_view>;
            { std::remove_cvref_t<W>::kind } -> std::convertible_to<node_kind>;
        };


        /// Copies from lvalues, moves from rvalues.
        template <widget W>
        auto take_data(W &&widget) -> node_data
        {
            if constexpr (std::is_lvalue_reference_v<W>)
                return widget.data_view();
            else
                return std::forward<W>(widget).release();
        }


        template <std::invocable<node_data &> F>
        struct modifier_fn
        {
            F fn;


            constexpr modifier_fn(F &&func) noexcept : fn { std::move(func) } {}
            constexpr modifier_fn(const F &func) noexcept : fn { func } {}

            void apply(node_data &data) const { fn(data); }
        };
    }


    template <node_kind Kind, _impl::widget_name Name = node_kind_to_string<Kind>()>
    class node
    {
    public:
        static constexpr auto name = Name.view();
        static constexpr auto kind = Kind;


        constexpr node(std::source_location origin) noexcept
            requires(kind != node_kind::text)
            : m_data { name, origin, kind }
        {
        }

        constexpr node(std::string text, std::source_location origin) noexcept
            requires(kind == node_kind::text)
            : m_data { name, origin, kind }
        { m_data.text = std::move(text); }


        void finalize(std::string_view parent_path = {}) noexcept { m_data.finalize(parent_path); }


        [[nodiscard]]
        constexpr auto data_view() const & noexcept -> const _impl::node_data &
        { return m_data; }


        [[nodiscard]]
        constexpr auto release() && noexcept -> _impl::node_data
        { return std::move(m_data); }


        template <typename Self, typename F>
        auto operator|(this Self &&self, const _impl::modifier_fn<F> &m)
            -> std::remove_cvref_t<Self>
        {
            auto s = prepare(self);
            m.apply(s.m_data);
            return s;
        }

        template <typename F>
        auto operator|=(const _impl::modifier_fn<F> &m) -> node &
        {
            m.apply(m_data);
            return *this;
        }

        template <typename Self, _impl::widget... W>
        auto operator[](this Self &&self, W &&...kids) -> std::remove_cvref_t<Self>
            requires(kind != node_kind::text)
        {
            auto s = prepare(self);
            s.m_data.children.reserve(s.m_data.children.size() + sizeof...(W));
            (s.m_data.children.emplace_back(_impl::take_data(std::forward<W>(kids))), ...);
            return s;
        }

    private:
        _impl::node_data m_data;


        template <typename Self>
        static auto prepare(Self &&self) -> std::remove_cvref_t<Self>
        { return std::forward<Self>(self); }
    };
}
