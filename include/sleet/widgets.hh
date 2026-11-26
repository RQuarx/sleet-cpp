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
}
