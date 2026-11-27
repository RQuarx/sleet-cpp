#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>


namespace sleet::txt
{
    /// One decoded UTF-8 code point.
    struct decoded_code_point
    {
        char32_t    value  = 0;
        std::size_t length = 1;
    };


    /// Decodes the code point starting at \p offset. Invalid or truncated
    /// sequences decode as one byte so callers always make progress.
    [[nodiscard]]
    auto decode_at(std::string_view text, std::size_t offset) noexcept -> decoded_code_point;


    /// Clamps a byte offset into \p text and moves it back to the start of a
    /// code point when it lands inside a multi-byte sequence.
    [[nodiscard]]
    auto clamp_offset(std::string_view text, std::size_t offset) noexcept -> std::size_t;


    /// Moves one grapheme cluster to the left. Combining marks, variation
    /// selectors, emoji modifiers, ZWJ sequences, and regional-indicator pairs
    /// stay together.
    [[nodiscard]]
    auto previous_offset(std::string_view text, std::size_t offset) noexcept -> std::size_t;


    /// Moves one grapheme cluster to the right.
    [[nodiscard]]
    auto next_offset(std::string_view text, std::size_t offset) noexcept -> std::size_t;
}
