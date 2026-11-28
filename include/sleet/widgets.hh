#pragma once
#include <source_location>
#include <string>
#include <vector>

#include "sleet/widget/control.hh"
#include "sleet/widget/node.hh"


namespace sleet::widget
{
    using box_node        = node<node_kind::box>;
    using column_node     = node<node_kind::column>;
    using row_node        = node<node_kind::row>;
    using label_node      = node<node_kind::text>;
    using text_input_node = node<node_kind::text>;
    using combo_box_node  = node<node_kind::column>;
    using button_node     = node<node_kind::box>;
    using checkbox_node   = node<node_kind::box>;
    using toggle_node     = node<node_kind::box>;
    using radio_node      = node<node_kind::box>;
    using slider_node     = node<node_kind::box>;
    using progress_node   = node<node_kind::row>;
    using separator_node  = node<node_kind::box>;
    using spacer_node     = node<node_kind::box>;
    using badge_node      = node<node_kind::text>;
    using list_view_node  = node<node_kind::column>;
    using list_item_node  = node<node_kind::box>;
    using table_view_node = node<node_kind::column>;
    using table_row_node  = node<node_kind::row>;
    using table_cell_node = node<node_kind::box>;
    using tree_view_node  = node<node_kind::column>;
    using tree_item_node  = node<node_kind::box>;


    /// The initial state of a tree row.
    struct tree_item_options
    {
        /// Zero for a root row; each level adds one indentation step.
        int depth = 0;

        /// Whether the row's children are currently shown.
        bool expanded = false;

        /// Whether the row has children, even while they are collapsed.
        bool has_children = false;

        /// The action reported when the row is expanded or collapsed. When
        /// empty, the row's normal action is used.
        std::string toggle_action;
    };


    /// A box that is laid out independently of its parent's flow and anchored
    /// inside the parent's content box. It is otherwise a normal box: sizing,
    /// padding, gaps, alignment, and children all behave like \c box_node.
    class floating_node : public node<node_kind::box>
    {
    public:
        explicit floating_node(std::source_location origin,
                               anchor_point         at       = anchor_point::top_left,
                               int                  stacking = 0)
            : node<node_kind::box>(origin)
        {
            auto &value            = data();
            value.floating         = true;
            value.placement_anchor = at;
            value.z_index          = stacking;
        }
    };


    /// Creates a floating box. The box is the root of its overlay subtree and
    /// can contain any normal layout widgets.
    /// \param at The parent edge or corner to anchor against.
    /// \param stacking The paint and hit-test order among floating nodes.
    /// \param origin The source location used for diagnostics.
    /// \return The floating box widget.
    [[nodiscard]]
    auto floating_box(anchor_point         at       = anchor_point::top_left,
                      int                  stacking = 0,
                      std::source_location origin   = std::source_location::current())
        -> floating_node;


    [[nodiscard]]
    auto box(std::source_location origin = std::source_location::current()) -> box_node;

    [[nodiscard]]
    auto row(std::source_location origin = std::source_location::current()) -> row_node;

    [[nodiscard]]
    auto column(std::source_location origin = std::source_location::current()) -> column_node;

    [[nodiscard]]
    auto label(std::string text = {}, std::source_location origin = std::source_location::current())
        -> label_node;

    /// A clickable button with a centered text label.
    /// \param text The label.
    /// \param action The action reported by clicks and keyboard activation.
    /// \param appearance The visual weight.
    /// \param origin The source location used for diagnostics.
    /// \return The button widget.
    [[nodiscard]]
    auto button(std::string          text,
                std::string          action,
                button_variant       appearance = button_variant::standard,
                std::source_location origin     = std::source_location::current()) -> button_node;

    /// A clickable button with arbitrary content.
    /// \param content The child content, usually a label or row.
    /// \param action The action reported by clicks and keyboard activation.
    /// \param appearance The visual weight.
    /// \param origin The source location used for diagnostics.
    /// \return The button widget.
    [[nodiscard]]
    auto button(node_data            content,
                std::string          action,
                button_variant       appearance = button_variant::standard,
                std::source_location origin     = std::source_location::current()) -> button_node;

    /// A single-line editable text field.
    ///
    /// The runtime owns the caret and selection. Edits are reported as
    /// \c event::kind::text_edit with the new value in \c event::text, and
    /// Enter is reported as \c event::kind::submit.
    /// \param value The current value from application state.
    /// \param action The action reported by edit and submit events.
    /// \param hint Placeholder text shown while the value is empty.
    /// \param origin The source location used for diagnostics.
    /// \return The text input widget.
    [[nodiscard]]
    auto text_input(std::string          value,
                    std::string          action,
                    std::string          hint   = {},
                    std::source_location origin = std::source_location::current())
        -> text_input_node;

    /// A multiline text editor with wrapped lines.
    ///
    /// Enter inserts a line break and Ctrl/Cmd+Enter reports
    /// \c event::kind::submit. Edits are reported as \c event::kind::text_edit
    /// with the full value in \c event::text. The default height is 120 points;
    /// apply \c height() to change it.
    /// \param value The current value from application state.
    /// \param action The action reported by edit and submit events.
    /// \param hint Placeholder text shown while the value is empty.
    /// \param origin The source location used for diagnostics.
    /// \return The text area widget.
    [[nodiscard]]
    auto text_area(std::string          value,
                   std::string          action,
                   std::string          hint   = {},
                   std::source_location origin = std::source_location::current())
        -> text_input_node;

    /// A combo box with an inline option list.
    ///
    /// Clicking the header opens the list. Choosing an option reports
    /// \c event::kind::selection_changed with the option text in
    /// \c event::text.
    /// \param value The current selection from application state.
    /// \param options The selectable options.
    /// \param action The action reported when an option is chosen.
    /// \param hint Placeholder text shown while no option is selected.
    /// \param origin The source location used for diagnostics.
    /// \return The combo box widget.
    [[nodiscard]]
    auto combo_box(std::string              value,
                   std::vector<std::string> options,
                   std::string              action,
                   std::string              hint = {},
                   std::source_location origin = std::source_location::current()) -> combo_box_node;

    /// A combo box whose options have explicit, stable identities. Use this
    /// overload when the option list is reordered or filtered between frames.
    /// \param option_ids One id per option; extra ids are ignored.
    [[nodiscard]]
    auto combo_box(std::string              value,
                   std::vector<std::string> options,
                   std::vector<widget_id>   option_ids,
                   std::string              action,
                   std::string              hint = {},
                   std::source_location origin = std::source_location::current()) -> combo_box_node;

    /// A checkbox with a text label.
    /// \param text The label.
    /// \param is_checked Whether the box is checked.
    /// \param action The action reported by clicks and keyboard activation.
    /// \param origin The source location used for diagnostics.
    /// \return The checkbox widget.
    [[nodiscard]]
    auto checkbox(std::string          text,
                  bool                 is_checked,
                  std::string          action,
                  std::source_location origin = std::source_location::current()) -> checkbox_node;

    /// A sliding on/off switch with a text label.
    /// \param text The label.
    /// \param is_checked Whether the switch is on.
    /// \param action The action reported by clicks and keyboard activation.
    /// \param origin The source location used for diagnostics.
    /// \return The toggle widget.
    [[nodiscard]]
    auto toggle(std::string          text,
                bool                 is_checked,
                std::string          action,
                std::source_location origin = std::source_location::current()) -> toggle_node;

    /// A radio button with a text label.
    /// \param text The label.
    /// \param is_selected Whether the button is selected.
    /// \param action The action reported by clicks and keyboard activation.
    /// \param origin The source location used for diagnostics.
    /// \return The radio button widget.
    [[nodiscard]]
    auto radio_button(std::string          text,
                      bool                 is_selected,
                      std::string          action,
                      std::source_location origin = std::source_location::current()) -> radio_node;

    /// A horizontal slider.
    ///
    /// Dragging or using the arrow keys reports \c event::kind::slider_changed
    /// with the normalized value in \c event::value.
    /// \param value The current value in the range zero to one.
    /// \param action The action reported by value changes.
    /// \param origin The source location used for diagnostics.
    /// \return The slider widget.
    [[nodiscard]]
    auto slider(float                value,
                std::string          action,
                std::source_location origin = std::source_location::current()) -> slider_node;

    /// A horizontal progress bar.
    /// \param value The progress in the range zero to one.
    /// \param bar_height The bar height in logical pixels.
    /// \param origin The source location used for diagnostics.
    /// \return The progress bar widget.
    [[nodiscard]]
    auto progress_bar(float                value,
                      float                bar_height = 8.F,
                      std::source_location origin     = std::source_location::current())
        -> progress_node;

    /// A one-pixel horizontal rule.
    /// \param origin The source location used for diagnostics.
    /// \return The separator widget.
    [[nodiscard]]
    auto separator(std::source_location origin = std::source_location::current()) -> separator_node;

    /// A flexible blank area that shares free space in both axes.
    /// \param origin The source location used for diagnostics.
    /// \return The spacer widget.
    [[nodiscard]]
    auto spacer(std::source_location origin = std::source_location::current()) -> spacer_node;

    /// A compact accent-colored label.
    /// \param text The badge text.
    /// \param origin The source location used for diagnostics.
    /// \return The badge widget.
    [[nodiscard]]
    auto badge(std::string text, std::source_location origin = std::source_location::current())
        -> badge_node;

    /// A full-width menu row with an optional selection mark.
    /// \param text The row label.
    /// \param action The action reported by clicks and keyboard activation.
    /// \param selected Shows a check mark before the label.
    /// \param origin The source location used for diagnostics.
    /// \return The menu row widget.
    [[nodiscard]]
    auto menu_item(std::string          text,
                   std::string          action,
                   bool                 selected = false,
                   std::source_location origin   = std::source_location::current()) -> button_node;

    /// A floating menu panel that holds \c menu_item rows. Add
    /// \c placement() to position it and mark it \c modal() to dismiss it when
    /// the pointer clicks outside.
    /// \param origin The source location used for diagnostics.
    /// \return The floating menu panel.
    [[nodiscard]]
    auto menu(std::source_location origin = std::source_location::current()) -> column_node;

    /// A vertically scrolling list surface. Rows are normally created with
    /// \c list_item. The view is one Tab stop; Up/Down, Home/End, and
    /// PageUp/PageDown move focus between its rows, and focus reveal scrolls
    /// the active row into view.
    /// \param origin The source location used for diagnostics.
    /// \return The list view widget.
    [[nodiscard]]
    auto list_view(std::source_location origin = std::source_location::current()) -> list_view_node;

    /// A selectable row inside a \c list_view. Clicking the row or pressing
    /// Enter or Space emits \c event::kind::activate with \p action. Pass
    /// \p is_selected to paint the row with the theme's selection color.
    /// \param text The row label.
    /// \param action The action reported by activation.
    /// \param is_selected Whether the row is selected.
    /// \param origin The source location used for diagnostics.
    /// \return The list row widget.
    [[nodiscard]]
    auto list_item(std::string          text,
                   std::string          action,
                   bool                 is_selected = false,
                   std::source_location origin = std::source_location::current()) -> list_item_node;

    /// A list row with arbitrary content.
    /// \param content The row content, usually a label or row of widgets.
    /// \param action The action reported by activation.
    /// \param is_selected Whether the row is selected.
    /// \param origin The source location used for diagnostics.
    /// \return The list row widget.
    [[nodiscard]]
    auto list_item(node_data            content,
                   std::string          action,
                   bool                 is_selected = false,
                   std::source_location origin = std::source_location::current()) -> list_item_node;

    /// A vertically scrolling table surface. It uses the same focus and
    /// keyboard navigation as \c list_view; rows are created with
    /// \c table_row and cells with \c table_cell.
    /// \param origin The source location used for diagnostics.
    /// \return The table view widget.
    [[nodiscard]]
    auto table_view(std::source_location origin = std::source_location::current())
        -> table_view_node;

    /// A selectable table row. Add \c table_cell children with the same
    /// weights used by the header so the columns line up.
    /// \param action The action reported by activation.
    /// \param is_selected Whether the row is selected.
    /// \param origin The source location used for diagnostics.
    /// \return The table row widget.
    [[nodiscard]]
    auto table_row(std::string          action,
                   bool                 is_selected = false,
                   std::source_location origin = std::source_location::current()) -> table_row_node;

    /// A non-selectable table header row. Add \c table_header_cell children
    /// with the same weights used by the data rows.
    /// \param origin The source location used for diagnostics.
    /// \return The table header widget.
    [[nodiscard]]
    auto table_header(std::source_location origin = std::source_location::current())
        -> table_row_node;

    /// A table cell with a weighted share of the row width.
    /// \param text The cell text.
    /// \param weight The relative column width; values are clamped to a small
    /// positive minimum.
    /// \param origin The source location used for diagnostics.
    /// \return The table cell widget.
    [[nodiscard]]
    auto table_cell(std::string          text,
                    float                weight = 1.F,
                    std::source_location origin = std::source_location::current())
        -> table_cell_node;

    /// A bold table header cell with a weighted share of the row width.
    /// \param text The header text.
    /// \param weight The relative column width; values are clamped to a small
    /// positive minimum.
    /// \param origin The source location used for diagnostics.
    /// \return The table header cell widget.
    [[nodiscard]]
    auto table_header_cell(std::string          text,
                           float                weight = 1.F,
                           std::source_location origin = std::source_location::current())
        -> table_cell_node;

    /// A vertically scrolling tree surface. Rows are normally created with
    /// \c tree_item and laid out as flat siblings whose \c tree_depth describes
    /// the hierarchy. Left/Right collapse, expand, and move to the parent or
    /// first child.
    /// \param origin The source location used for diagnostics.
    /// \return The tree view widget.
    [[nodiscard]]
    auto tree_view(std::source_location origin = std::source_location::current()) -> tree_view_node;

    /// A selectable tree row. Clicking the row or pressing Enter or Space emits
    /// \c event::kind::activate with \p action. Clicking the disclosure area or
    /// pressing Left/Right emits the toggle action when the row has children.
    /// \param text The row label.
    /// \param action The action reported by activation.
    /// \param options The row's depth, expansion state, and toggle action.
    /// \param origin The source location used for diagnostics.
    /// \return The tree row widget.
    [[nodiscard]]
    auto tree_item(std::string          text,
                   std::string          action,
                   tree_item_options    options = {},
                   std::source_location origin = std::source_location::current()) -> tree_item_node;

    /// A tree row with arbitrary content.
    /// \param content The row content, usually a label or row of widgets.
    /// \param action The action reported by activation.
    /// \param options The row's depth, expansion state, and toggle action.
    /// \param origin The source location used for diagnostics.
    /// \return The tree row widget.
    [[nodiscard]]
    auto tree_item(node_data            content,
                   std::string          action,
                   tree_item_options    options = {},
                   std::source_location origin = std::source_location::current()) -> tree_item_node;
}
