#include <algorithm>
#include <limits>
#include <vector>

#include "sleet/error.hh"
#include "sleet/text/grapheme.hh"
#include "sleet/text/line_breaking.hh"
#include "sleet/text/wrap.hh"


namespace
{
    struct chosen_break
    {
        std::size_t            offset = 0;
        sleet::txt::break_kind kind   = sleet::txt::break_kind::allowed;
    };


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
    auto measure_width(std::string_view text, const sleet::txt::line_measurer &measure_line)
        -> kei::result<float>
    {
        auto width = measure_line(trim_line_end(text));
        if (!width) return std::move(width.error()).unexpected();
        return *width;
    }


    /// Finds the longest prefix that fits when a word is wider than the
    /// available space. Breaking at grapheme boundaries keeps combining marks
    /// and emoji sequences intact. Prefix widths grow monotonically, so a
    /// binary search avoids measuring every boundary.
    [[nodiscard]]
    auto break_inside_word(std::string_view                 text,
                           std::size_t                      start,
                           std::size_t                      limit,
                           float                            width_limit,
                           const sleet::txt::line_measurer &measure_line)
        -> kei::result<chosen_break>
    {
        auto next = sleet::txt::next_offset(text, start);
        if (next > limit) return chosen_break { next, sleet::txt::break_kind::allowed };

        // Collect boundaries in small chunks. The whole word can be far wider
        // than the line, so gathering every boundary up to the next break
        // opportunity made wrap-anywhere quadratic for long words.
        constexpr auto chunk = std::size_t { 16 };

        std::vector<std::size_t> boundaries;
        boundaries.push_back(start);

        const auto collect = [&](std::size_t count)
        {
            while (count > 0 and next <= limit)
            {
                boundaries.push_back(next);
                if (next >= limit)
                {
                    next = limit + 1;
                    break;
                }

                next = sleet::txt::next_offset(text, next);
                --count;
            }
        };

        auto last_fitting = std::size_t { 0 };

        while (true)
        {
            const auto before = boundaries.size();
            collect(chunk);
            if (boundaries.size() == before) break;

            const auto last = boundaries.size() - 1;
            auto width = measure_width(text.substr(start, boundaries[last] - start), measure_line);
            if (!width) return std::move(width.error()).unexpected();

            if (*width <= width_limit)
            {
                last_fitting = last;
                if (next > limit) break;
                continue;
            }

            // The last boundary in this chunk overflows. All boundaries up to
            // last_fitting fit, so binary search the unknown range.
            auto lo = last_fitting;
            auto hi = last;
            while (lo + 1 < hi)
            {
                const auto mid = lo + ((hi - lo) / 2);
                auto       candidate
                    = measure_width(text.substr(start, boundaries[mid] - start), measure_line);
                if (!candidate) return std::move(candidate.error()).unexpected();

                if (*candidate <= width_limit)
                    lo = mid;
                else
                    hi = mid;
            }

            auto best = boundaries[lo];
            if (best == start) best = boundaries[1];
            return chosen_break { best, sleet::txt::break_kind::allowed };
        }

        return chosen_break { boundaries[last_fitting], sleet::txt::break_kind::allowed };
    }


    [[nodiscard]]
    auto choose_break(std::string_view                                  text,
                      std::size_t                                       start,
                      const std::vector<sleet::txt::break_opportunity> &opportunities,
                      std::size_t                                       first_index,
                      std::size_t                                       mandatory_index,
                      std::optional<float>                              width_limit,
                      const sleet::txt::line_measurer                  &measure_line,
                      bool break_anywhere) -> kei::result<chosen_break>
    {
        // The caller keeps this cursor advancing between lines. Rescanning
        // from the beginning for every line made long paragraphs quadratic.
        const auto first = opportunities.begin() + static_cast<std::ptrdiff_t>(first_index);
        if (first == opportunities.end())
            return chosen_break { text.size(), sleet::txt::break_kind::mandatory };

        // A line never crosses the next mandatory break. Its index is also
        // maintained by the caller so this does not rescan the whole tail of
        // the paragraph on every line.
        const auto last = mandatory_index == opportunities.size()
                            ? opportunities.end() - 1
                            : opportunities.begin() + static_cast<std::ptrdiff_t>(mandatory_index);

        // Without a width limit, only mandatory breaks matter; no measurement
        // is needed to choose the line.
        if (!width_limit) return chosen_break { last->offset, last->kind };

        // When words may be split, find the longest grapheme prefix that fits
        // before measuring break opportunities. Measuring the first
        // opportunity directly would shape the entire remaining word on every
        // line, which is quadratic for long unbroken words.
        if (break_anywhere)
        {
            auto prefix = break_inside_word(text, start, last->offset, *width_limit, measure_line);
            if (!prefix) return std::move(prefix.error()).unexpected();

            auto after = std::upper_bound(
                first, last + 1, prefix->offset,
                [](std::size_t offset, const sleet::txt::break_opportunity &opportunity)
                { return offset < opportunity.offset; });

            if (after != first)
            {
                --after;
                return chosen_break { after->offset, after->kind };
            }

            return prefix;
        }

        // Prefix widths are monotonic for a single line, so the last candidate
        // that fits can be found with a binary search. This keeps wrapping a
        // long paragraph at O(log n) measurements per line instead of
        // re-measuring every prefix.
        auto       best      = std::numeric_limits<std::size_t>::max();
        auto       best_kind = sleet::txt::break_kind::allowed;
        const auto count     = static_cast<std::size_t>(std::distance(first, last)) + 1;
        auto       lo        = std::size_t { 0 };
        auto       hi        = count;

        while (lo < hi)
        {
            const auto  mid         = lo + ((hi - lo) / 2);
            const auto &opportunity = *(first + static_cast<std::ptrdiff_t>(mid));
            const auto  candidate   = trim_line_end(text.substr(start, opportunity.offset - start));

            auto width = measure_line(candidate);
            if (!width) return std::move(width.error()).unexpected();

            if (*width <= *width_limit)
            {
                best      = opportunity.offset;
                best_kind = opportunity.kind;
                lo        = mid + 1;
            }
            else
                hi = mid;
        }

        if (best != std::numeric_limits<std::size_t>::max())
            return chosen_break { best, best_kind };

        // A single unbreakable word overflows instead of disappearing.
        return chosen_break { first->offset, first->kind };
    }
}


auto sleet::txt::wrap(std::string_view     text,
                      std::optional<float> max_width,
                      const line_measurer &measure_line,
                      bool                 break_anywhere) -> kei::result<std::vector<wrapped_line>>
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
                                   measure_line, break_anywhere);
        if (!chosen) return std::move(chosen.error()).unexpected();

        // Every path advances the cursor, but guard against a malformed break
        // table so the loop cannot spin forever.
        if (chosen->offset <= start) chosen->offset = next_offset(text, start);
        if (chosen->offset <= start) break;

        const auto line = trim_line_end(text.substr(start, chosen->offset - start));

        // A soft break can land on leading whitespace when a word is split at
        // grapheme boundaries. Consume it instead of emitting a blank line;
        // mandatory breaks still represent real paragraph gaps.
        if (line.empty() and chosen->kind != break_kind::mandatory and chosen->offset < text.size())
        {
            start = chosen->offset;
            continue;
        }

        auto width = measure_line(line);
        if (!width) return std::move(width.error()).unexpected();

        lines.push_back({ line, *width,
                          chosen->kind == break_kind::mandatory or chosen->offset >= text.size(),
                          start });
        start = chosen->offset;
    }

    return lines;
}
