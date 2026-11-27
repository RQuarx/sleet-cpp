#include <algorithm>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

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


    /// Assigns every copied field from one place. Keeping four hand-written
    /// copies of this list made adding a widget property error-prone.
    template <typename Other>
    void assign_fields(node_data &self, Other &&other)
    {
        self.id               = std::forward<Other>(other).id;
        self.text             = std::forward<Other>(other).text;
        self.action           = std::forward<Other>(other).action;
        self.children         = std::forward<Other>(other).children;
        self.name             = std::forward<Other>(other).name;
        self.size             = std::forward<Other>(other).size;
        self.margin           = std::forward<Other>(other).margin;
        self.padding          = std::forward<Other>(other).padding;
        self.origin           = std::forward<Other>(other).origin;
        self.font             = std::forward<Other>(other).font;
        self.wrap             = std::forward<Other>(other).wrap;
        self.text_align       = std::forward<Other>(other).text_align;
        self.multiline        = std::forward<Other>(other).multiline;
        self.gap              = std::forward<Other>(other).gap;
        self.alignment        = std::forward<Other>(other).alignment;
        self.main_axis        = std::forward<Other>(other).main_axis;
        self.align_self       = std::forward<Other>(other).align_self;
        self.flex             = std::forward<Other>(other).flex;
        self.kind             = std::forward<Other>(other).kind;
        self.visual           = std::forward<Other>(other).visual;
        self.visible          = std::forward<Other>(other).visible;
        self.motion           = std::forward<Other>(other).motion;
        self.floating         = std::forward<Other>(other).floating;
        self.placement_anchor = std::forward<Other>(other).placement_anchor;
        self.placement        = std::forward<Other>(other).placement;
        self.z_index          = std::forward<Other>(other).z_index;
        self.clip_children    = std::forward<Other>(other).clip_children;
        self.scrollable       = std::forward<Other>(other).scrollable;
        self.show_scrollbar   = std::forward<Other>(other).show_scrollbar;
        self.reveal_on_focus  = std::forward<Other>(other).reveal_on_focus;
        self.disabled         = std::forward<Other>(other).disabled;
        self.control          = std::forward<Other>(other).control;
        self.tint             = std::forward<Other>(other).tint;
        self.variant          = std::forward<Other>(other).variant;
        self.focusable        = std::forward<Other>(other).focusable;
        self.autofocus        = std::forward<Other>(other).autofocus;
        self.checked          = std::forward<Other>(other).checked;
        self.control_value    = std::forward<Other>(other).control_value;
        self.placeholder      = std::forward<Other>(other).placeholder;
        self.tooltip_text     = std::forward<Other>(other).tooltip_text;
        self.backdrop         = std::forward<Other>(other).backdrop;
        self.focus_scope      = std::forward<Other>(other).focus_scope;
        self.focus_group      = std::forward<Other>(other).focus_group;
        self.modal            = std::forward<Other>(other).modal;
        self.cancel_action    = std::forward<Other>(other).cancel_action;
        self.shortcuts        = std::forward<Other>(other).shortcuts;
        self.shortcut_error   = std::forward<Other>(other).shortcut_error;
        self.background_tint  = std::forward<Other>(other).background_tint;
        self.border_tint      = std::forward<Other>(other).border_tint;
    }
}


node_data::node_data(std::string_view widget_name, std::source_location source, node_kind node_type)
    : name { widget_name }, origin { source }, kind { node_type }
{
}


node_data::node_data(const node_data &other)
{
    assign_fields(*this, other);
    refresh_parents(false);
}


node_data::node_data(node_data &&other) noexcept
{
    assign_fields(*this, std::move(other));
    refresh_parents(false);
}


auto node_data::operator=(const node_data &other) -> node_data &
{
    if (this == &other) return *this;

    assign_fields(*this, other);
    refresh_parents(false);
    return *this;
}


auto node_data::operator=(node_data &&other) noexcept -> node_data &
{
    if (this == &other) return *this;

    assign_fields(*this, std::move(other));
    refresh_parents(false);
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
    // Ids are assigned before they are inserted and never move afterwards, so
    // the duplicate check can view them instead of copying every id.
    std::unordered_set<std::string_view> seen;
    seen.reserve(children_amount() + 1);
    return finalize_impl(seen, parent_path);
}


auto node_data::finalize_impl(std::unordered_set<std::string_view> &seen,
                              std::string_view                      parent_path) -> kei::result<>
{
    if (id.empty())
    {
        if (parent_path.empty())
            id = widget_id { generated_base(*this) };
        else
            id = widget_id { std::format("{}/{}", parent_path, generated_base(*this)) };
    }

    if (!seen.insert(id.view()).second)
        return sleet::error { "Duplicate widget id \"{}\".", id.view() }.unexpected();

    if (!shortcut_error.empty())
        return sleet::error { "Widget \"{}\": {}.", id.view(), shortcut_error }.unexpected();

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


auto sleet::widget::text_alignment_of(const node_data &node) noexcept -> text_alignment
{
    if (node.text_align) return *node.text_align;

    switch (node.alignment.x)
    {
    case align::center: return text_alignment::center;
    case align::end:    return text_alignment::right;
    case align::start:  break;
    }

    return text_alignment::left;
}


auto sleet::widget::text_alignment_offset(text_alignment alignment, float free_space) noexcept
    -> float
{
    const auto free = std::max(0.F, free_space);

    switch (alignment)
    {
    case text_alignment::left:    return 0.F;
    case text_alignment::center:  return free / 2.F;
    case text_alignment::right:   return free;
    case text_alignment::justify: return 0.F;
    }

    return 0.F;
}
