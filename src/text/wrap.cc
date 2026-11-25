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
                      std::optional<float>                              width_limit,
                      const sleet::txt::line_measurer &measure_line) -> kei::result<std::size_t>
    {
        std::size_t last_fit = std::numeric_limits<std::size_t>::max();
        std::size_t chosen   = text.size();

        for (const auto &opportunity : opportunities)
        {
            if (opportunity.offset <= start) continue;

            const auto candidate = trim_line_end(text.substr(start, opportunity.offset - start));

            auto width = measure_line(candidate);
            if (!width) return std::move(width.error()).unexpected();

            const bool mandatory = opportunity.kind == sleet::txt::break_kind::mandatory
                                or opportunity.offset >= text.size();

            if (width_limit and *width > *width_limit)
            {
                chosen = last_fit == std::numeric_limits<std::size_t>::max() ? opportunity.offset
                                                                             : last_fit;
                break;
            }

            last_fit = opportunity.offset;

            if (mandatory)
            {
                chosen = opportunity.offset;
                break;
            }
        }

        if (chosen <= start) chosen = text.size();
        return chosen;
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

    std::size_t start = 0;
    while (start < text.size())
    {
        auto chosen = choose_break(text, start, *opportunities, width_limit, measure_line);
        if (!chosen) return std::move(chosen.error()).unexpected();

        const auto line = trim_line_end(text.substr(start, *chosen - start));

        auto width = measure_line(line);
        if (!width) return std::move(width.error()).unexpected();

        lines.push_back({ line, *width });
        start = *chosen;
    }

    return lines;
}
