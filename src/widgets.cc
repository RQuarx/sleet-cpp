#include "sleet/widgets.hh"


namespace sleet::widget
{
    auto box(std::source_location origin) noexcept -> box_node { return { origin }; }
    auto column(std::source_location origin) noexcept -> column_node { return { origin }; }
    auto row(std::source_location origin) noexcept -> row_node { return { origin }; }
    auto label(std::string text, std::source_location origin) noexcept -> label_node
    { return label_node { std::move(text), origin }; }
}
