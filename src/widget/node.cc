#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

#include "sleet/widget/node.hh"


using sleet::widget::node_data;


namespace
{
    /// A compact, deterministic suffix for generated ids. The call site is
    /// stable across re-renders, so inserting or reordering unrelated siblings
    /// no longer changes the identity of a widget.
    [[nodiscard]]
    auto origin_suffix(const std::source_location &origin) -> std::string
    {
        auto file = std::string_view { origin.file_name() == nullptr ? "" : origin.file_name() };
        if (const auto separator = file.find_last_of("/\\"); separator != std::string_view::npos)
            file.remove_prefix(separator + 1);

        return std::format("{}:{}:{}", file, origin.line(), origin.column());
    }


    [[nodiscard]]
    auto generated_base(const node_data &node) -> std::string
    {
        auto base = std::format("{}::{}@{}", sleet::widget::node_kind_to_string(node.kind),
                                node.name, origin_suffix(node.origin));

        // Combo options are commonly reordered or filtered. Including their
        // label keeps the identity attached to the value instead of its index.
        if (node.control == sleet::widget::control_kind::combo_option and !node.text.empty())
            base += std::format("#{}", node.text);

        return base;
    }
}


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
      motion { other.motion }, floating { other.floating },
      placement_anchor { other.placement_anchor }, placement { other.placement },
      z_index { other.z_index }, clip_children { other.clip_children },
      scrollable { other.scrollable }, show_scrollbar { other.show_scrollbar },
      reveal_on_focus { other.reveal_on_focus }, disabled { other.disabled },
      control { other.control }, tint { other.tint }, variant { other.variant },
      focusable { other.focusable }, checked { other.checked },
      control_value { other.control_value }, placeholder { other.placeholder },
      tooltip_text { other.tooltip_text }, backdrop { other.backdrop },
      focus_scope { other.focus_scope }, modal { other.modal },
      cancel_action { other.cancel_action }, background_tint { other.background_tint },
      border_tint { other.border_tint }
{ refresh_parents(true); }


node_data::node_data(node_data &&other) noexcept
    : id { std::move(other.id) }, text { std::move(other.text) },
      action { std::move(other.action) }, children { std::move(other.children) },
      name { std::move(other.name) }, size { other.size }, margin { other.margin },
      padding { other.padding }, origin { other.origin }, font { other.font }, gap { other.gap },
      alignment { other.alignment }, main_axis { other.main_axis }, align_self { other.align_self },
      flex { other.flex }, kind { other.kind }, visual { other.visual }, visible { other.visible },
      motion { other.motion }, floating { other.floating },
      placement_anchor { other.placement_anchor }, placement { other.placement },
      z_index { other.z_index }, clip_children { other.clip_children },
      scrollable { other.scrollable }, show_scrollbar { other.show_scrollbar },
      reveal_on_focus { other.reveal_on_focus }, disabled { other.disabled },
      control { other.control }, tint { other.tint }, variant { other.variant },
      focusable { other.focusable }, checked { other.checked },
      control_value { other.control_value }, placeholder { std::move(other.placeholder) },
      tooltip_text { std::move(other.tooltip_text) }, backdrop { other.backdrop },
      focus_scope { other.focus_scope }, modal { other.modal },
      cancel_action { std::move(other.cancel_action) }, background_tint { other.background_tint },
      border_tint { other.border_tint }
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
    motion     = other.motion;

    floating         = other.floating;
    placement_anchor = other.placement_anchor;
    placement        = other.placement;
    z_index          = other.z_index;
    clip_children    = other.clip_children;
    scrollable       = other.scrollable;
    show_scrollbar   = other.show_scrollbar;
    reveal_on_focus  = other.reveal_on_focus;
    disabled         = other.disabled;

    control       = other.control;
    tint          = other.tint;
    variant       = other.variant;
    focusable     = other.focusable;
    checked       = other.checked;
    control_value = other.control_value;
    placeholder   = other.placeholder;

    tooltip_text    = other.tooltip_text;
    backdrop        = other.backdrop;
    focus_scope     = other.focus_scope;
    modal           = other.modal;
    cancel_action   = other.cancel_action;
    background_tint = other.background_tint;
    border_tint     = other.border_tint;

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
    motion     = other.motion;

    floating         = other.floating;
    placement_anchor = other.placement_anchor;
    placement        = other.placement;
    z_index          = other.z_index;
    clip_children    = other.clip_children;
    scrollable       = other.scrollable;
    show_scrollbar   = other.show_scrollbar;
    reveal_on_focus  = other.reveal_on_focus;
    disabled         = other.disabled;

    control       = other.control;
    tint          = other.tint;
    variant       = other.variant;
    focusable     = other.focusable;
    checked       = other.checked;
    control_value = other.control_value;
    placeholder   = std::move(other.placeholder);

    tooltip_text    = std::move(other.tooltip_text);
    backdrop        = other.backdrop;
    focus_scope     = other.focus_scope;
    modal           = other.modal;
    cancel_action   = std::move(other.cancel_action);
    background_tint = other.background_tint;
    border_tint     = other.border_tint;

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
            id = widget_id { generated_base(*this) };
        else
            id = widget_id { std::format("{}/{}", parent_path, generated_base(*this)) };
    }

    if (!seen.insert(id.str()).second)
        return sleet::error { "Duplicate widget id \"{}\".", id.view() }.unexpected();

    // Occurrence numbers are scoped to one parent and one call site. Widgets
    // created in a loop are still distinguishable, but a widget from a
    // different call site never shifts an existing id.
    std::unordered_map<std::string, std::size_t> occurrences;

    for (auto &child : children)
    {
        child.parent = this;

        if (child.id.empty())
        {
            const auto base   = generated_base(child);
            const auto count  = occurrences[base]++;
            const auto suffix = count == 0 ? std::string {} : std::format("#{}", count);
            child.id          = widget_id { std::format("{}/{}{}", id.view(), base, suffix) };
        }

        if (auto result = child.finalize_impl(seen, id.view()); !result)
            return std::move(result.error()).unexpected();
    }

    return {};
}
