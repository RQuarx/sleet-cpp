#include <algorithm>
#include <cstdint>

#include "sleet/text/grapheme.hh"
#include "sleet/text/ustring.hh"
#include "sleet/widget/control.hh"


namespace
{
    [[nodiscard]]
    constexpr auto is_word_code_point(char32_t value) noexcept -> bool
    {
        return value == U'_' or (value >= U'0' and value <= U'9')
            or (value >= U'A' and value <= U'Z') or (value >= U'a' and value <= U'z')
            or value >= 0x80U;
    }
}


auto sleet::widget::clamp_offset(std::string_view text, std::size_t offset) noexcept -> std::size_t
{ return txt::clamp_offset(text, offset); }


auto sleet::widget::previous_offset(std::string_view text, std::size_t offset) noexcept
    -> std::size_t
{ return txt::previous_offset(text, offset); }


auto sleet::widget::next_offset(std::string_view text, std::size_t offset) noexcept -> std::size_t
{ return txt::next_offset(text, offset); }


auto sleet::widget::previous_word_offset(std::string_view text, std::size_t offset) noexcept
    -> std::size_t
{
    auto position = clamp_offset(text, offset);

    while (position > 0)
    {
        const auto previous = previous_offset(text, position);
        if (is_word_code_point(txt::decode_at(text, previous).value)) break;
        position = previous;
    }

    while (position > 0)
    {
        const auto previous = previous_offset(text, position);
        if (!is_word_code_point(txt::decode_at(text, previous).value)) break;
        position = previous;
    }

    return position;
}


auto sleet::widget::next_word_offset(std::string_view text, std::size_t offset) noexcept
    -> std::size_t
{
    auto position = clamp_offset(text, offset);
    if (position >= text.size()) return text.size();

    if (is_word_code_point(txt::decode_at(text, position).value))
    {
        // Inside a word, stop at its end so punctuation is its own step.
        while (position < text.size() and is_word_code_point(txt::decode_at(text, position).value))
            position = next_offset(text, position);
        return position;
    }

    while (position < text.size() and !is_word_code_point(txt::decode_at(text, position).value))
        position = next_offset(text, position);

    return position;
}


auto sleet::widget::word_range_at(std::string_view text, std::size_t offset) noexcept
    -> std::pair<std::size_t, std::size_t>
{
    auto position = clamp_offset(text, offset);

    if (position < text.size() and !is_word_code_point(txt::decode_at(text, position).value))
    {
        const auto first = position;
        while (position < text.size() and !is_word_code_point(txt::decode_at(text, position).value))
            position = next_offset(text, position);
        return { first, position };
    }

    while (position > 0)
    {
        const auto previous = previous_offset(text, position);
        if (!is_word_code_point(txt::decode_at(text, previous).value)) break;
        position = previous;
    }

    const auto first = position;
    while (position < text.size() and is_word_code_point(txt::decode_at(text, position).value))
        position = next_offset(text, position);

    return { first, position };
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


auto sleet::widget::erase_word_backward(std::string &text, text_selection &selection) noexcept
    -> bool
{
    if (selection.has_selection()) return erase_backward(text, selection);
    if (selection.caret == 0) return false;

    const auto previous = previous_word_offset(text, selection.caret);
    text.erase(previous, selection.caret - previous);
    selection.collapse(previous);
    return true;
}


auto sleet::widget::erase_word_forward(std::string &text, text_selection &selection) noexcept
    -> bool
{
    if (selection.has_selection()) return erase_forward(text, selection);
    if (selection.caret >= text.size()) return false;

    const auto next = next_word_offset(text, selection.caret);
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


auto sleet::widget::sanitize_multiline(std::string_view text) -> std::string
{
    if (!txt::ustring::from_utf8(text)) return {};

    std::string result;
    result.reserve(text.size());

    for (std::size_t i = 0; i < text.size(); i++)
    {
        const auto value = static_cast<unsigned char>(text[i]);

        if (value == '\r')
        {
            result.push_back('\n');
            if (i + 1 < text.size() and text[i + 1] == '\n') i++;
            continue;
        }

        if (value == '\n')
        {
            result.push_back('\n');
            continue;
        }

        if (value < 0x20U or value == 0x7FU)
        {
            result.push_back(' ');
            continue;
        }

        result.push_back(text[i]);
    }

    return result;
}
