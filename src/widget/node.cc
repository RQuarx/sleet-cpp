#include "sleet/widget/node.hh"

using sleet::widget::_impl::node_data;


node_data::node_data(std::string_view name, std::source_location origin, node_kind kind) noexcept
    : name { name }, origin { origin }, kind { kind }
{
}


node_data::node_data(node_data &&o) noexcept
    : id { std::move(o.id) }, text { std::move(o.text) }, children { std::move(o.children) },
      name { o.name }, size { o.size }, margin { o.margin }, padding { o.padding },
      origin { o.origin }, font { o.font }, alignment { o.alignment }, kind { o.kind }
{ refresh_parents(false); }


node_data::node_data(const node_data &o)
    : id { o.id }, text { o.text }, children { o.children }, name { o.name }, size { o.size },
      margin { o.margin }, padding { o.padding }, origin { o.origin }, font { o.font },
      alignment { o.alignment }, kind { o.kind }
{ refresh_parents(false); }


auto node_data::children_amount(bool recurse) const noexcept -> std::size_t
{
    std::size_t n = children.size();

    if (recurse)
        for (const auto &child : children) n += child.children_amount(recurse);
    return n;
}


void node_data::refresh_parents(bool recurse) noexcept
{
    for (auto &child : children)
    {
        child.parent = this;
        if (recurse) child.refresh_parents(recurse);
    }
}


void node_data::finalize(std::string_view parent_path) noexcept
{
    if (id.view().empty())
        id = widget_id { parent_path.empty()
                             ? std::format("{}::{}", node_kind_to_string(kind), name)
                             : std::format("{}/{}::{}", parent_path, node_kind_to_string(kind),
                                           name) };

    for (std::size_t i = 0; i < children.size(); i++)
    {
        auto &child  = children[i];
        child.parent = this;

        // The index disambiguates siblings of the same kind and name. A child with an
        // explicit id keeps it; finalize() only fills in empty ones.
        if (child.id.view().empty())
            child.id = widget_id { std::format("{}/{}::{}-{}", id.view(),
                                               node_kind_to_string(child.kind), child.name, i) };

        child.finalize(id.view());
    }
}
