#include <algorithm>
#include <cstdint>

#include "sleet/text/ustring.hh"
#include "sleet/widget/control.hh"


namespace
{
    [[nodiscard]]
    auto is_continuation(std::string_view text, std::size_t offset) noexcept -> bool
    {
        if (offset >= text.size()) return false;
        return (static_cast<unsigned char>(text[offset]) & 0xC0U) == 0x80U;
    }
}


auto sleet::widget::clamp_offset(std::string_view text, std::size_t offset) noexcept -> std::size_t
{
    if (offset >= text.size()) return text.size();

    // An offset inside a multi-byte sequence snaps back to the lead byte.
    while (offset > 0 and is_continuation(text, offset)) offset--;
    return offset;
}


auto sleet::widget::previous_offset(std::string_view text, std::size_t offset) noexcept
    -> std::size_t
{
    offset = clamp_offset(text, offset);
    if (offset == 0) return 0;

    offset--;
    while (offset > 0 and is_continuation(text, offset)) offset--;
    return offset;
}


auto sleet::widget::next_offset(std::string_view text, std::size_t offset) noexcept -> std::size_t
{
    offset = clamp_offset(text, offset);
    if (offset >= text.size()) return text.size();

    offset++;
    while (offset < text.size() and is_continuation(text, offset)) offset++;
    return offset;
}


void sleet::widget::select_all(std::string_view text, text_selection &selection) noexcept
{
    selection.anchor = 0;
    selection.caret  = text.size();
}


void sleet::widget::move_caret(std::string_view text,
                               text_selection  &selection,
                               std::size_t      offset,
                               bool             extend) noexcept
{
    selection.caret = clamp_offset(text, offset);
    if (!extend) selection.anchor = selection.caret;
}


void
sleet::widget::insert_text(std::string &text, text_selection &selection, std::string_view insert)
{
    const auto first = selection.begin();
    const auto last  = selection.end();

    text.replace(first, last - first, insert);
    selection.collapse(first + insert.size());
}


auto sleet::widget::erase_backward(std::string &text, text_selection &selection) noexcept -> bool
{
    if (selection.has_selection())
    {
        text.erase(selection.begin(), selection.end() - selection.begin());
        selection.collapse(selection.begin());
        return true;
    }

    if (selection.caret == 0) return false;

    const auto previous = previous_offset(text, selection.caret);
    text.erase(previous, selection.caret - previous);
    selection.collapse(previous);
    return true;
}


auto sleet::widget::erase_forward(std::string &text, text_selection &selection) noexcept -> bool
{
    if (selection.has_selection())
    {
        const auto first = selection.begin();
        text.erase(first, selection.end() - first);
        selection.collapse(first);
        return true;
    }

    if (selection.caret >= text.size()) return false;

    const auto next = next_offset(text, selection.caret);
    text.erase(selection.caret, next - selection.caret);
    return true;
}


auto sleet::widget::selected_text(std::string_view text, const text_selection &selection)
    -> std::string
{
    if (!selection.has_selection()) return {};
    return std::string { text.substr(selection.begin(), selection.end() - selection.begin()) };
}


auto sleet::widget::sanitize_single_line(std::string_view text) -> std::string
{
    if (!txt::ustring::from_utf8(text)) return {};

    std::string result;
    result.reserve(text.size());

    for (std::size_t i = 0; i < text.size(); i++)
    {
        const auto value = static_cast<unsigned char>(text[i]);

        // A CRLF pair becomes one space so pasted Windows line endings do not
        // double up.
        if (value == '\r')
        {
            result.push_back(' ');
            if (i + 1 < text.size() and text[i + 1] == '\n') i++;
            continue;
        }

        if (value == '\n' or value < 0x20U or value == 0x7FU)
        {
            result.push_back(' ');
            continue;
        }

        result.push_back(text[i]);
    }

    return result;
}
