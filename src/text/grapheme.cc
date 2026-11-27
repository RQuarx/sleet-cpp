#include "sleet/text/grapheme.hh"


namespace
{
    using sleet::txt::decoded_code_point;


    [[nodiscard]]
    auto is_continuation(std::string_view text, std::size_t offset) noexcept -> bool
    {
        if (offset >= text.size()) return false;
        return (static_cast<unsigned char>(text[offset]) & 0xC0U) == 0x80U;
    }


    [[nodiscard]]
    auto decode_before(std::string_view text, std::size_t offset) noexcept -> decoded_code_point
    {
        if (offset == 0) return {};

        auto start = offset - 1;
        while (start > 0 and is_continuation(text, start)) start--;
        return sleet::txt::decode_at(text, start);
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
}


auto sleet::txt::decode_at(std::string_view text, std::size_t offset) noexcept -> decoded_code_point
{
    if (offset >= text.size()) return {};

    const auto lead = static_cast<unsigned char>(text[offset]);

    if (lead < 0x80U) return { static_cast<char32_t>(lead), 1 };

    const auto available = text.size() - offset;
    if ((lead & 0xE0U) == 0xC0U and available >= 2)
        return { static_cast<char32_t>(((lead & 0x1FU) << 6U)
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


auto sleet::txt::clamp_offset(std::string_view text, std::size_t offset) noexcept -> std::size_t
{
    if (offset >= text.size()) return text.size();

    // An offset inside a multi-byte sequence snaps back to the lead byte.
    while (offset > 0 and is_continuation(text, offset)) offset--;
    return offset;
}


auto sleet::txt::previous_offset(std::string_view text, std::size_t offset) noexcept -> std::size_t
{
    offset = clamp_offset(text, offset);
    if (offset == 0) return 0;

    auto current = decode_before(text, offset);
    auto start   = offset - current.length;

    while (start > 0)
    {
        auto before = decode_before(text, start);

        // A combining mark or trailing joiner belongs to the base character
        // before it.
        if (is_extend(current.value))
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


auto sleet::txt::next_offset(std::string_view text, std::size_t offset) noexcept -> std::size_t
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
