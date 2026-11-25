#pragma once
#include <cstddef>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <utility>


namespace sleet::widget
{
    /// A widget identity as users see it. Equality uses the path alone, and all
    /// empty ids hash to 0 so hashing stays consistent with equality.
    class widget_id
    {
    public:
        widget_id() = default;

        widget_id(std::string id) noexcept
            : m_path { std::move(id) }, m_hash { std::hash<std::string> {}(m_path) }
        {
        }

        widget_id(const char *id)
            : widget_id { id == nullptr ? std::string {} : std::string { id } }
        {
        }


        [[nodiscard]]
        auto empty() const noexcept -> bool
        { return m_path.empty(); }

        [[nodiscard]] auto view() const noexcept -> std::string_view { return m_path; }
        [[nodiscard]] auto str() const noexcept -> const std::string & { return m_path; }

        [[nodiscard]]
        auto hash() const noexcept -> std::size_t
        { return m_path.empty() ? 0 : m_hash; }

        [[nodiscard]]
        friend auto operator==(const widget_id &lhs, const widget_id &rhs) noexcept -> bool
        { return lhs.m_path == rhs.m_path; }

    private:
        std::string m_path;
        std::size_t m_hash = 0;
    };
}


template <>
struct std::hash<sleet::widget::widget_id>
{
    auto operator()(const sleet::widget::widget_id &id) const noexcept -> std::size_t
    { return id.hash(); }
};


template <>
struct std::formatter<sleet::widget::widget_id>
{
    template <class ParseContext>
    constexpr auto parse(ParseContext &ctx) const noexcept -> ParseContext::iterator
    { return ctx.begin(); }

    template <class FmtContext>
    auto format(const sleet::widget::widget_id &id, FmtContext &ctx) const -> FmtContext::iterator
    { return std::format_to(ctx.out(), "{}", id.view()); }
};
