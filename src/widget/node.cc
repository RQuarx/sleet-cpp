#include <stdexcept>

#include "sleet/widget/node.hh"


using sleet::widget::node_data;


node_data::node_data(std::string_view name, std::source_location origin, node_kind kind)
    : name { name }, origin { origin }, kind { kind }
{
}


node_data::node_data(const node_data &other)
    : id { other.id }, text { other.text }, action { other.action }, children { other.children },
      name { other.name }, size { other.size }, margin { other.margin }, padding { other.padding },
      origin { other.origin }, font { other.font }, gap { other.gap },
      alignment { other.alignment }, main_axis { other.main_axis }, align_self { other.align_self },
      flex { other.flex }, kind { other.kind }, visual { other.visual }, visible { other.visible },
      control { other.control }, tint { other.tint }, variant { other.variant },
      focusable { other.focusable }, checked { other.checked },
      control_value { other.control_value }, placeholder { other.placeholder }
{ refresh_parents(true); }


node_data::node_data(node_data &&other) noexcept
    : id { std::move(other.id) }, text { std::move(other.text) },
      action { std::move(other.action) }, children { std::move(other.children) },
      name { std::move(other.name) }, size { other.size }, margin { other.margin },
      padding { other.padding }, origin { other.origin }, font { other.font }, gap { other.gap },
      alignment { other.alignment }, main_axis { other.main_axis }, align_self { other.align_self },
      flex { other.flex }, kind { other.kind }, visual { other.visual }, visible { other.visible },
      control { other.control }, tint { other.tint }, variant { other.variant },
      focusable { other.focusable }, checked { other.checked },
      control_value { other.control_value }, placeholder { std::move(other.placeholder) }
{ refresh_parents(true); }


auto node_data::operator=(const node_data &other) -> node_data &
{
    if (this == &other) return *this;

    id         = other.id;
    text       = other.text;
    action     = other.action;
    children   = other.children;
    name       = other.name;
    size       = other.size;
    margin     = other.margin;
    padding    = other.padding;
    origin     = other.origin;
    font       = other.font;
    gap        = other.gap;
    alignment  = other.alignment;
    main_axis  = other.main_axis;
    align_self = other.align_self;
    flex       = other.flex;
    kind       = other.kind;
    visual     = other.visual;
    visible    = other.visible;

    control       = other.control;
    tint          = other.tint;
    variant       = other.variant;
    focusable     = other.focusable;
    checked       = other.checked;
    control_value = other.control_value;
    placeholder   = other.placeholder;

    refresh_parents(true);
    return *this;
}


auto node_data::operator=(node_data &&other) noexcept -> node_data &
{
    if (this == &other) return *this;

    id         = std::move(other.id);
    text       = std::move(other.text);
    action     = std::move(other.action);
    children   = std::move(other.children);
    name       = std::move(other.name);
    size       = other.size;
    margin     = other.margin;
    padding    = other.padding;
    origin     = other.origin;
    font       = other.font;
    gap        = other.gap;
    alignment  = other.alignment;
    main_axis  = other.main_axis;
    align_self = other.align_self;
    flex       = other.flex;
    kind       = other.kind;
    visual     = other.visual;
    visible    = other.visible;

    control       = other.control;
    tint          = other.tint;
    variant       = other.variant;
    focusable     = other.focusable;
    checked       = other.checked;
    control_value = other.control_value;
    placeholder   = std::move(other.placeholder);

    refresh_parents(true);
    return *this;
}


auto node_data::children_amount(bool recurse) const noexcept -> std::size_t
{
    std::size_t amount = children.size();
    if (recurse)
        for (const auto &child : children) amount += child.children_amount(true);
    return amount;
}


void node_data::refresh_parents(bool recurse) noexcept
{
    for (auto &child : children)
    {
        child.parent = this;
        if (recurse) child.refresh_parents(true);
    }
}


auto node_data::find(std::string_view key) noexcept -> node_data *
{
    if (id.view() == key) return this;

    for (auto &child : children)
        if (auto *result = child.find(key); result != nullptr) return result;

    return nullptr;
}


auto node_data::find(std::string_view key) const noexcept -> const node_data *
{ return const_cast<node_data *>(this)->find(key); }


auto node_data::find(const char *key) noexcept -> node_data *
{
    if (key == nullptr) return nullptr;
    return find(std::string_view { key });
}

auto node_data::find(const char *key) const noexcept -> const node_data *
{
    if (key == nullptr) return nullptr;
    return find(std::string_view { key });
}

auto node_data::find(const widget_id &key) noexcept -> node_data * { return find(key.view()); }


auto node_data::find(const widget_id &key) const noexcept -> const node_data *
{ return find(key.view()); }


auto node_data::operator[](std::string_view key) -> node_data &
{
    if (auto *result = find(key); result != nullptr) return *result;

    throw std::out_of_range { std::format("Widget id \"{}\" was not found.", key) };
}


auto node_data::operator[](std::string_view key) const -> const node_data &
{ return const_cast<node_data *>(this)->operator[](key); }


auto node_data::finalize(std::string_view parent_path) -> kei::result<>
{
    std::unordered_set<std::string> seen;
    seen.reserve(children_amount() + 1);
    return finalize_impl(seen, parent_path);
}


auto node_data::finalize_impl(std::unordered_set<std::string> &seen, std::string_view parent_path)
    -> kei::result<>
{
    if (id.empty())
    {
        if (parent_path.empty())
            id = widget_id { std::format("{}::{}", node_kind_to_string(kind), name) };
        else
            id = widget_id { std::format("{}/{}::{}", parent_path, node_kind_to_string(kind),
                                         name) };
    }

    if (!seen.insert(id.str()).second)
        return sleet::error { "Duplicate widget id \"{}\".", id.view() }.unexpected();

    for (std::size_t i = 0; i < children.size(); i++)
    {
        auto &child  = children[i];
        child.parent = this;

        if (child.id.empty())
            child.id = widget_id { std::format("{}/{}::{}-{}", id.view(),
                                               node_kind_to_string(child.kind), child.name, i) };

        if (auto result = child.finalize_impl(seen, id.view()); !result)
            return std::move(result.error()).unexpected();
    }

    return {};
}
