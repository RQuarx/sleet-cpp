#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>


namespace sleet::widget
{
    /// The built-in behaviour attached to a node. Most widgets are pure
    /// compositions, but editable and selectable controls need a little runtime
    /// state that lives outside the view function.
    enum class control_kind : std::uint8_t
    {
        none,
        text_input,
        combo_box,
        combo_option,
        slider,
        checkbox_indicator,
        radio_indicator,
    };


    /// A semantic color role resolved by the active theme.
    enum class tone : std::uint8_t
    {
        none,
        base,
        field,
        muted,
        accent,
        accent_fg,
        danger,
        danger_fg,
        border,
    };


    /// The visual weight of a button.
    enum class button_variant : std::uint8_t
    {
        standard,
        primary,
        ghost,
        danger,
    };


    /// A caret and selection anchor stored as byte offsets into UTF-8 text.
    /// Both offsets always sit on a code point boundary.
    struct text_selection
    {
        std::size_t caret  = 0;
        std::size_t anchor = 0;


        [[nodiscard]]
        constexpr auto has_selection() const noexcept -> bool
        { return caret != anchor; }

        [[nodiscard]]
        constexpr auto begin() const noexcept -> std::size_t
        { return caret < anchor ? caret : anchor; }

        [[nodiscard]]
        constexpr auto end() const noexcept -> std::size_t
        { return caret < anchor ? anchor : caret; }

        constexpr void collapse(std::size_t offset) noexcept { caret = anchor = offset; }
    };


    /// Clamps a byte offset into \p text and moves it back to the start of a
    /// code point when it lands inside a multi-byte sequence.
    /// \param text Valid UTF-8 text.
    /// \param offset The requested byte offset.
    /// \return A safe byte offset.
    [[nodiscard]]
    auto clamp_offset(std::string_view text, std::size_t offset) noexcept -> std::size_t;

    /// Moves one grapheme cluster to the left. Combining marks, variation
    /// selectors, emoji modifiers, ZWJ sequences, and regional-indicator pairs
    /// stay together.
    /// \param text Valid UTF-8 text.
    /// \param offset A byte offset on a code point boundary.
    /// \return The previous boundary, or zero at the start.
    [[nodiscard]]
    auto previous_offset(std::string_view text, std::size_t offset) noexcept -> std::size_t;

    /// Moves one grapheme cluster to the right.
    /// \param text Valid UTF-8 text.
    /// \param offset A byte offset on a code point boundary.
    /// \return The next boundary, or the end of the text.
    [[nodiscard]]
    auto next_offset(std::string_view text, std::size_t offset) noexcept -> std::size_t;

    /// Moves to the start of the previous word. Whitespace and punctuation are
    /// skipped first, then the word itself.
    [[nodiscard]]
    auto previous_word_offset(std::string_view text, std::size_t offset) noexcept -> std::size_t;

    /// Moves to the end of the current word, or past the current run of
    /// non-word characters.
    [[nodiscard]]
    auto next_word_offset(std::string_view text, std::size_t offset) noexcept -> std::size_t;

    /// Returns the word surrounding \p offset, or the adjacent run of
    /// non-word characters when the caret is not inside a word.
    [[nodiscard]]
    auto word_range_at(std::string_view text, std::size_t offset) noexcept
        -> std::pair<std::size_t, std::size_t>;

    /// Selects the whole string.
    /// \param text The text to select.
    /// \param selection The selection to update.
    void select_all(std::string_view text, text_selection &selection) noexcept;

    /// Moves the caret and optionally extends the selection.
    /// \param text Valid UTF-8 text.
    /// \param selection The selection to update.
    /// \param offset The target byte offset.
    /// \param extend Whether the anchor should stay in place.
    void move_caret(std::string_view text,
                    text_selection  &selection,
                    std::size_t      offset,
                    bool             extend) noexcept;

    /// Replaces the selected text, or inserts at the caret when nothing is
    /// selected. The caret ends after the inserted text.
    /// \param text The text to edit.
    /// \param selection The selection to update.
    /// \param insert The UTF-8 text to insert.
    void insert_text(std::string &text, text_selection &selection, std::string_view insert);

    /// Deletes the selection, or the code point before the caret.
    /// \param text The text to edit.
    /// \param selection The selection to update.
    /// \return \c true when the text changed.
    auto erase_backward(std::string &text, text_selection &selection) noexcept -> bool;

    /// Deletes the selection, or the code point after the caret.
    /// \param text The text to edit.
    /// \param selection The selection to update.
    /// \return \c true when the text changed.
    auto erase_forward(std::string &text, text_selection &selection) noexcept -> bool;

    /// Deletes the selection, or the word before the caret.
    auto erase_word_backward(std::string &text, text_selection &selection) noexcept -> bool;

    /// Deletes the selection, or the word after the caret.
    auto erase_word_forward(std::string &text, text_selection &selection) noexcept -> bool;

    /// Copies the selected range.
    /// \param text The source text.
    /// \param selection The active selection.
    /// \return The selected bytes, or an empty string when nothing is selected.
    [[nodiscard]]
    auto selected_text(std::string_view text, const text_selection &selection) -> std::string;

    /// Normalizes clipboard or input text for a single-line field. Line breaks
    /// and other control characters are replaced with spaces, and invalid
    /// UTF-8 is rejected.
    /// \param text The candidate text.
    /// \return Valid single-line UTF-8 text, or an empty string when \p text is
    /// not valid UTF-8.
    [[nodiscard]]
    auto sanitize_single_line(std::string_view text) -> std::string;
}
