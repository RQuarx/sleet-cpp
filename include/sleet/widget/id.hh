#pragma once
#include <kei-utils/result.hh>


namespace sleet::widget
{
    class widget_id
    {
    public:
        widget_id() = default;
        widget_id(std::string id) noexcept
            : m_path { std::move(id) }, m_hash { std::hash<std::string> {}(m_path) }
        {
        }

        auto operator==(const widget_id &) const -> bool = default;


        [[nodiscard]] auto view() const noexcept -> std::string_view { return m_path; }
        [[nodiscard]] auto hash() const noexcept -> std::size_t { return m_hash; }

    private:
        std::string m_path;
        std::size_t m_hash;
    };
}


template <>
struct std::hash<sleet::widget::widget_id>
{
    auto operator()(const sleet::widget::widget_id &id) const noexcept -> std::size_t
    { return id.hash(); }
};
