#pragma once
#include <functional>

#include "sleet/math/rect.hh"
#include "sleet/widget/id.hh"
#include "sleet/widget/node.hh"


namespace sleet::widget
{
    /// A function used by @ref calculate_layout to get text sizes.
    ///
    /// ### Parameters
    /// - `std::string_view text`
    /// - `font_spec font`
    /// - `std::optional<float> width_limit`
    ///
    /// ### Returns
    /// `kei::result<math::vec2>`
    using text_measurer = std::function<
        auto(std::string_view, font_spec, std::optional<float>)->kei::result<math::vec2>>;


    /// Calculates the layout of the widget nodes.
    ///
    /// @note `root` must be finalized first before this function is called.
    [[nodiscard]]
    auto calculate_layout(const _impl::node_data &root, const text_measurer &text_measurer) noexcept
        -> kei::result<std::unordered_map<std::string_view, math::rect>>;
}
