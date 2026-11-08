#pragma once
#include "widget/node.hh"


namespace sleet::widget
{
    using box_node    = node<node_kind::box>;
    using column_node = node<node_kind::column>;
    using row_node    = node<node_kind::row>;
    using label_node  = node<node_kind::text>;


    [[nodiscard]]
    auto box(std::source_location origin = std::source_location::current()) noexcept -> box_node;

    [[nodiscard]]
    auto row(std::source_location origin = std::source_location::current()) noexcept -> row_node;

    [[nodiscard]]
    auto column(std::source_location origin = std::source_location::current()) noexcept
        -> column_node;

    [[nodiscard]]
    auto label(std::string          text   = {},
               std::source_location origin = std::source_location::current()) noexcept
        -> label_node;
}
