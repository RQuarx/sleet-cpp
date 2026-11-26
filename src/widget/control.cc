#include <algorithm>
#include <cstdint>

#include "sleet/text/ustring.hh"
#include "sleet/widget/control.hh"


namespace
{
    struct decoded_code_point
    {
        char32_t    value  = 0;
        std::size_t length = 1;
    };


    [[nodiscard]]
    auto is_continuation(std::string_view text, std::size_t offset) noexcept -> bool
    {
        if (offset >= text.size()) return false;
        return (static_cast<unsigned char>(text[offset]) & 0xC0U) == 0x80U;
    }


    [[nodiscard]]
    auto decode_at(std::string_view text, std::size_t offset) noexcept -> decoded_code_point
    {
        if (offset >= text.size()) return {};

        const auto lead = static_cast<unsigned char>(text[offset]);

        if (lead < 0x80U) return { static_cast<char32_t>(lead), 1 };

        const auto available = text.size() - offset;
        if ((lead & 0xE0U) == 0xC0U and available >= 2)
            return { static_cast<char32_t>(
                         ((lead & 0x1FU) << 6U)
                         | (static_cast<unsigned char>(text[offset + 1]) & 0x3FU)),
                     2 };

        if ((lead & 0xF0U) == 0xE0U and available >= 3)
            return { static_cast<char32_t>(
                         ((lead & 0x0FU) << 12U)
                         | ((static_cast<unsigned char>(text[offset + 1]) & 0x3FU) << 6U)
                         | (static_cast<unsigned char>(text[offset + 2]) & 0x3FU)),
                     3 };

        if ((lead & 0xF8U) == 0xF0U and available >= 4)
            return { static_cast<char32_t>(
                         ((lead & 0x07U) << 18U)
                         | ((static_cast<unsigned char>(text[offset + 1]) & 0x3FU) << 12U)
                         | ((static_cast<unsigned char>(text[offset + 2]) & 0x3FU) << 6U)
                         | (static_cast<unsigned char>(text[offset + 3]) & 0x3FU)),
                     4 };

        return { static_cast<char32_t>(lead), 1 };
    }


    [[nodiscard]]
    auto decode_before(std::string_view text, std::size_t offset) noexcept -> decoded_code_point
    {
        if (offset == 0) return {};

        auto start = offset - 1;
        while (start > 0 and is_continuation(text, start)) start--;
        return decode_at(text, start);
    }


    /// True for code points that extend the preceding grapheme cluster.
    [[nodiscard]]
    constexpr auto is_extend(char32_t value) noexcept -> bool
    {
        return (value >= 0x0300U and value <= 0x036FU) // combining diacritics
            or (value >= 0x0483U and value <= 0x0489U) // Cyrillic marks
            or (value >= 0x0591U and value <= 0x05BDU) // Hebrew points
            or (value >= 0x0610U and value <= 0x061AU) // Arabic marks
            or (value >= 0x064BU and value <= 0x065FU)
            or (value >= 0x0900U and value <= 0x0903U) // Indic vowel signs
            or (value >= 0x093AU and value <= 0x094FU) or (value >= 0x1AB0U and value <= 0x1AFFU)
            or (value >= 0x1DC0U and value <= 0x1DFFU) or (value >= 0x20D0U and value <= 0x20FFU)
            or (value >= 0xFE00U and value <= 0xFE0FU) // variation selectors
            or (value >= 0xFE20U and value <= 0xFE2FU)
            or (value >= 0x1F3FBU and value <= 0x1F3FFU) // emoji skin tones
            or (value >= 0xE0020U and value <= 0xE007FU) // emoji tag characters
            or (value >= 0xE0100U and value <= 0xE01EFU) or value == 0x200DU; // zero-width joiner
    }


    [[nodiscard]]
    constexpr auto is_regional_indicator(char32_t value) noexcept -> bool
    { return value >= 0x1F1E6U and value <= 0x1F1FFU; }


    /// Counts contiguous regional indicators immediately before \p offset.
    /// Flags pair from the start of a run, so a run of three indicators is
    /// one flag plus a lone indicator rather than one lone indicator plus a
    /// flag.
    [[nodiscard]]
    auto preceding_regional_indicators(std::string_view text, std::size_t offset) noexcept
        -> std::size_t
    {
        auto count    = std::size_t { 0 };
        auto position = offset;

        while (position > 0)
        {
            const auto previous = decode_before(text, position);
            if (!is_regional_indicator(previous.value)) break;

            ++count;
            position -= previous.length;
        }

        return count;
    }


    [[nodiscard]]
    constexpr auto is_word_code_point(char32_t value) noexcept -> bool
    {
        return value == U'_' or (value >= U'0' and value <= U'9')
            or (value >= U'A' and value <= U'Z') or (value >= U'a' and value <= U'z')
            or value >= 0x80U;
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

    auto current = decode_before(text, offset);
    auto start   = offset - current.length;

    while (start > 0)
    {
        auto before = decode_before(text, start);

        // A combining mark belongs to the base character before it.
        if (is_extend(current.value) and current.value != 0x200DU)
        {
            start  -= before.length;
            current = before;
            continue;
        }

        // Include the base and joiner of a ZWJ sequence.
        if (before.value == 0x200DU)
        {
            start -= before.length;
            if (start > 0)
            {
                auto base = decode_before(text, start);
                start    -= base.length;
                current   = base;
            }
            continue;
        }

        // Keep a regional-indicator flag pair together. Pair from the start of
        // the run: in a three-indicator run, the last indicator is alone.
        if (is_regional_indicator(current.value) and is_regional_indicator(before.value))
        {
            if (preceding_regional_indicators(text, start) % 2 == 1) start -= before.length;
            break;
        }

        if (current.value == U'\n' and before.value == U'\r')
        {
            start  -= before.length;
            current = before;
            continue;
        }

        break;
    }

    return start;
}


auto sleet::widget::next_offset(std::string_view text, std::size_t offset) noexcept -> std::size_t
{
    offset = clamp_offset(text, offset);
    if (offset >= text.size()) return text.size();

    const auto first = decode_at(text, offset);
    auto       next  = offset + first.length;

    if (first.value == U'\r' and next < text.size() and text[next] == '\n') return next + 1;

    auto current = first.value;

    while (next < text.size())
    {
        const auto candidate = decode_at(text, next);

        if (is_extend(candidate.value) and candidate.value != 0x200DU)
        {
            next += candidate.length;
            continue;
        }

        if (current == 0x200DU or candidate.value == 0x200DU)
        {
            next   += candidate.length;
            current = candidate.value;
            continue;
        }

        if (is_regional_indicator(current) and is_regional_indicator(candidate.value))
        {
            // Consume the second indicator only when this offset starts a
            // pair; otherwise the indicator is the lone tail of a run.
            if (preceding_regional_indicators(text, offset) % 2 == 0) next += candidate.length;
            break;
        }

        break;
    }

    return next;
}


auto sleet::widget::previous_word_offset(std::string_view text, std::size_t offset) noexcept
    -> std::size_t
{
    auto position = clamp_offset(text, offset);

    while (position > 0)
    {
        const auto previous = previous_offset(text, position);
        if (is_word_code_point(decode_at(text, previous).value)) break;
        position = previous;
    }

    while (position > 0)
    {
        const auto previous = previous_offset(text, position);
        if (!is_word_code_point(decode_at(text, previous).value)) break;
        position = previous;
    }

    return position;
}


auto sleet::widget::next_word_offset(std::string_view text, std::size_t offset) noexcept
    -> std::size_t
{
    auto position = clamp_offset(text, offset);
    if (position >= text.size()) return text.size();

    if (is_word_code_point(decode_at(text, position).value))
    {
        // Inside a word, stop at its end so punctuation is its own step.
        while (position < text.size() and is_word_code_point(decode_at(text, position).value))
            position = next_offset(text, position);
        return position;
    }

    while (position < text.size() and !is_word_code_point(decode_at(text, position).value))
        position = next_offset(text, position);

    return position;
}


auto sleet::widget::word_range_at(std::string_view text, std::size_t offset) noexcept
    -> std::pair<std::size_t, std::size_t>
{
    auto position = clamp_offset(text, offset);

    if (position < text.size() and !is_word_code_point(decode_at(text, position).value))
    {
        const auto first = position;
        while (position < text.size() and !is_word_code_point(decode_at(text, position).value))
            position = next_offset(text, position);
        return { first, position };
    }

    while (position > 0)
    {
        const auto previous = previous_offset(text, position);
        if (!is_word_code_point(decode_at(text, previous).value)) break;
        position = previous;
    }

    const auto first = position;
    while (position < text.size() and is_word_code_point(decode_at(text, position).value))
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
