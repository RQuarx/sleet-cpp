#include <algorithm>
#include <limits>

#include "sleet/error.hh"
#include "sleet/text/line_breaking.hh"
#include "sleet/text/wrap.hh"


namespace
{
    [[nodiscard]]
    auto trim_line_end(std::string_view line) noexcept -> std::string_view
    {
        while (!line.empty())
        {
            const auto c = line.back();
            if (c != ' ' and c != '\t' and c != '\r' and c != '\n') break;
            line.remove_suffix(1);
        }
        return line;
    }


    [[nodiscard]]
    auto choose_break(std::string_view                                  text,
                      std::size_t                                       start,
                      const std::vector<sleet::txt::break_opportunity> &opportunities,
                      std::size_t                                       first_index,
                      std::size_t                                       mandatory_index,
                      std::optional<float>                              width_limit,
                      const sleet::txt::line_measurer &measure_line) -> kei::result<std::size_t>
    {
        // The caller keeps this cursor advancing between lines. Rescanning
        // from the beginning for every line made long paragraphs quadratic.
        const auto first = opportunities.begin() + static_cast<std::ptrdiff_t>(first_index);
        if (first == opportunities.end()) return text.size();

        // A line never crosses the next mandatory break. Its index is also
        // maintained by the caller so this does not rescan the whole tail of
        // the paragraph on every line.
        const auto last = mandatory_index == opportunities.size()
                            ? opportunities.end() - 1
                            : opportunities.begin() + static_cast<std::ptrdiff_t>(mandatory_index);

        // Without a width limit, only mandatory breaks matter; no measurement
        // is needed to choose the line.
        if (!width_limit) return last->offset;

        // Prefix widths are monotonic for a single line, so the last candidate
        // that fits can be found with a binary search. This keeps wrapping a
        // long paragraph at O(log n) measurements per line instead of
        // re-measuring every prefix.
        auto       best  = std::numeric_limits<std::size_t>::max();
        const auto count = static_cast<std::size_t>(std::distance(first, last)) + 1;
        auto       lo    = std::size_t { 0 };
        auto       hi    = count;

        while (lo < hi)
        {
            const auto  mid         = lo + ((hi - lo) / 2);
            const auto &opportunity = *(first + static_cast<std::ptrdiff_t>(mid));
            const auto  candidate   = trim_line_end(text.substr(start, opportunity.offset - start));

            auto width = measure_line(candidate);
            if (!width) return std::move(width.error()).unexpected();

            if (*width <= *width_limit)
            {
                best = opportunity.offset;
                lo   = mid + 1;
            }
            else
                hi = mid;
        }

        // A single unbreakable word overflows instead of disappearing. The
        // caller will measure the resulting line once more.
        if (best == std::numeric_limits<std::size_t>::max()) return first->offset;
        return best;
    }
}


auto sleet::txt::wrap(std::string_view     text,
                      std::optional<float> max_width,
                      const line_measurer &measure_line) -> kei::result<std::vector<wrapped_line>>
{
    if (!measure_line) return sleet::error { "wrap() requires a line measurer." }.unexpected();

    std::vector<wrapped_line> lines;
    if (text.empty()) return lines;

    auto opportunities = break_opportunity::of(text);
    if (!opportunities) return std::move(opportunities.error()).unexpected();

    if (opportunities->empty() or opportunities->back().offset < text.size())
        opportunities->emplace_back(text.size(), break_kind::mandatory);

    // A supplied limit is always honoured. std::nullopt is the only way to ask
    // for unwrapped text. Zero and negative limits are clamped to zero, so the
    // text breaks at every opportunity rather than overflowing indefinitely.
    const auto width_limit
        = max_width.transform([](float width) noexcept { return std::max(0.F, width); });

    std::size_t start     = 0;
    std::size_t cursor    = 0;
    std::size_t mandatory = 0;

    const auto advance_to_mandatory = [&opportunities](std::size_t index)
    {
        while (index < opportunities->size()
               and (*opportunities)[index].kind != sleet::txt::break_kind::mandatory)
            ++index;
        return index;
    };

    mandatory = advance_to_mandatory(0);

    while (start < text.size())
    {
        while (cursor < opportunities->size() and (*opportunities)[cursor].offset <= start)
            ++cursor;

        while (mandatory < opportunities->size() and (*opportunities)[mandatory].offset <= start)
            mandatory = advance_to_mandatory(mandatory + 1);

        if (cursor == opportunities->size()) break;

        auto chosen = choose_break(text, start, *opportunities, cursor, mandatory, width_limit,
                                   measure_line);
        if (!chosen) return std::move(chosen.error()).unexpected();

        const auto line = trim_line_end(text.substr(start, *chosen - start));

        auto width = measure_line(line);
        if (!width) return std::move(width.error()).unexpected();

        lines.push_back({ line, *width });
        start = *chosen;
    }

    return lines;
}
