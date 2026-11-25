#pragma once
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

#include <kei-utils/result.hh>


namespace sleet::txt
{
    struct wrapped_line
    {
        std::string_view text;
        float            width = 0.F;
    };


    /// Measures the width of a single line of text.
    using line_measurer = std::function<auto(std::string_view)->kei::result<float>>;


    /// Splits \c text into lines at mandatory breaks. When \c max_width has a
    /// value, it also breaks at the last allowed opportunity that still fits. A
    /// supplied width is clamped to zero, and \c std::nullopt disables wrapping.
    /// Trailing spaces and newlines are removed from each line.
    /// \param text The text to wrap.
    /// \param max_width The maximum line width, or \c std::nullopt to disable
    /// wrapping.
    /// \param measure_line Measures the width of a candidate line.
    /// \return The wrapped lines, or an error from \p measure_line.
    [[nodiscard]]
    auto wrap(std::string_view     text,
              std::optional<float> max_width,
              const line_measurer &measure_line) -> kei::result<std::vector<wrapped_line>>;
}
