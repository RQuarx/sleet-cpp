#pragma once
#include <functional>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "sleet/math/rect.hh"
#include "sleet/math/vec2.hh"
#include "sleet/widget/node.hh"
#include "sleet/widget/scroll.hh"


namespace sleet::widget
{
    /// Measures text for \c calculate_layout. \c text is the string to measure,
    /// \c font selects the face and style, \c width_limit asks for wrapping
    /// when it has a value, and \c wrap selects the wrapping strategy. The
    /// result is the text size in logical pixels, including any line wrapping.
    using text_measurer = std::function<
        auto(std::string_view, const font_spec &, std::optional<float>, text_wrap_mode)
            ->kei::result<math::vec2>>;


    /// The original three-argument measurer. It is adapted to \c text_measurer
    /// and cannot split words; \c text_wrap_mode::wrap_anywhere falls back to
    /// ordinary wrapping.
    using basic_text_measurer = std::function<
        auto(std::string_view, const font_spec &, std::optional<float>)->kei::result<math::vec2>>;


    struct floating_entry
    {
        widget_id id;
        int       z_index = 0;
    };


    struct layout_result
    {
        math::rect root;

        std::unordered_map<widget_id, math::rect> rects;

        /// Floating nodes in paint order (ascending z-index, then document
        /// order). They are painted above all in-flow content. IDs are stored
        /// instead of pointers so a layout can safely outlive its tree.
        std::vector<floating_entry> floating;


        [[nodiscard]]
        auto find(const widget_id &id) const noexcept -> const math::rect *
        {
            const auto it = rects.find(id);
            return it == rects.end() ? nullptr : &it->second;
        }

        [[nodiscard]]
        auto find(std::string_view id) const -> const math::rect *
        { return find(widget_id { std::string { id } }); }

        [[nodiscard]]
        auto find(const char *id) const -> const math::rect *
        { return id == nullptr ? nullptr : find(std::string_view { id }); }
    };


    /// Calculates the layout of the widget nodes.
    ///
    /// \param root The finalized root node.
    /// \param measure_text The callback used to measure text.
    /// \param viewport The size of the root content box. A root with \c fit
    /// sizing uses its intrinsic size, while a root with \c grow or \c fill
    /// sizing fills the viewport.
    /// \return The resolved layout, or an error.
    /// \pre \p root must be finalized before calling this function.
    [[nodiscard]]
    auto calculate_layout(const node_data     &root,
                          const text_measurer &measure_text,
                          math::vec2           viewport) -> kei::result<layout_result>;


    /// Compatibility overload for the original three-argument measurer.
    [[nodiscard]]
    auto calculate_layout(const node_data           &root,
                          const basic_text_measurer &measure_text,
                          math::vec2                 viewport) -> kei::result<layout_result>;
}
