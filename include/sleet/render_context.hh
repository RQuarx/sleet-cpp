#pragma once
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL_render.h>

#include "sleet/math/vec2.hh"
#include "sleet/widget/control.hh"
#include "sleet/widget/layout.hh"
#include "sleet/widget/scroll.hh"
#include "sleet/widget/theme.hh"


namespace sleet
{
    /// The eased interaction state of one widget. Values are in \c [0, 1].
    /// The runtime updates these while transitions are in flight.
    struct interaction_progress
    {
        float hover = 0.F;
        float press = 0.F;
        float focus = 0.F;
    };


    /// One transient notification drawn in the bottom-right corner.
    struct toast_message
    {
        std::string text;
        float       opacity = 1.F;

        /// Additional downward offset in logical pixels, used for slide-in.
        float offset = 0.F;
    };


    /// Owns the SDL renderer resources needed to draw a finalized widget tree:
    /// fonts, the text texture cache, and the active theme.
    ///
    /// The constructor throws \c kei::error if the renderer is invalid or no
    /// font is available. Rendering reports failures through \c kei::result
    /// instead of throwing.
    class render_context
    {
    public:
        render_context(SDL_Renderer *renderer, widget::theme theme);
        ~render_context();

        render_context(const render_context &)            = delete;
        render_context &operator=(const render_context &) = delete;
        render_context(render_context &&) noexcept;
        render_context &operator=(render_context &&) noexcept;


        [[nodiscard]]
        auto measure_text(std::string_view         text,
                          const widget::font_spec &font,
                          std::optional<float>     width_limit,
                          widget::text_wrap_mode   wrap = widget::text_wrap_mode::wrap)
            -> kei::result<math::vec2>;

        [[nodiscard]]
        auto render(const widget::node_data &root, const widget::layout_result &layout)
            -> kei::result<>;

        /// Replaces the theme and clears cached fonts and textures.
        /// \param theme The new theme to install.
        /// \throws kei::error if the new body font cannot be opened, after
        /// restoring the previous theme.
        void set_theme(widget::theme theme);
        void set_hovered(std::optional<widget::widget_id> id) noexcept;
        void set_pressed(std::optional<widget::widget_id> id) noexcept;

        /// Sets eased interaction progress per widget. When set, the renderer
        /// blends hover, press, and focus feedback by these amounts instead of
        /// switching instantly.
        void set_interaction_progress(
            std::unordered_map<widget::widget_id, interaction_progress> progress) noexcept;

        /// Sets the scroll state used to draw scrollbars for scrollable nodes.
        void set_scroll_metrics(
            std::unordered_map<widget::widget_id, widget::scroll_metrics> metrics) noexcept;

        /// Shows a tooltip bubble below \p anchor. An empty text clears it.
        void set_tooltip(std::string text, math::rect anchor);
        void clear_tooltip() noexcept;

        /// Sets the transient notifications painted above everything else.
        void set_toasts(std::vector<toast_message> toasts);

        /// Sets the widget that owns keyboard focus. Focused widgets draw a
        /// focus-colored border.
        /// \param id The focused widget, or \c std::nullopt when nothing is
        /// focused.
        void set_focused(std::optional<widget::widget_id> id) noexcept;

        /// Sets the caret and selection drawn by a focused text input.
        /// \param id The text input widget.
        /// \param selection The caret and anchor byte offsets.
        void set_text_selection(std::optional<widget::widget_id> id,
                                widget::text_selection           selection) noexcept;

        void set_viewport(math::vec2 viewport) noexcept;

        /// Sets the ratio of physical output pixels to logical layout points.
        /// Layout stays in points; fonts, shapes, and glyph positions are
        /// rasterized at \p scale so high-DPI windows stay sharp.
        void set_pixel_scale(float scale) noexcept;

        /// Sets the in-progress IME composition shown by a text input.
        /// \param id The editing text input, or \c std::nullopt to clear it.
        /// \param text The composition text.
        /// \param caret The caret byte offset inside \p text.
        void set_preedit(std::optional<widget::widget_id> id,
                         std::string                      text,
                         std::size_t                      caret) noexcept;

        /// Returns the caret rectangle of a text input in logical window
        /// coordinates. The runtime uses it to place IME candidate windows.
        /// \return The caret rectangle, or \c std::nullopt when the node is not
        /// a text input or its font cannot be measured.
        [[nodiscard]]
        auto text_input_caret_rect(const widget::node_data      &node,
                                   const math::rect             &bounds,
                                   const widget::text_selection &selection)
            -> std::optional<math::rect>;

    private:
        struct impl;
        std::unique_ptr<impl> m_impl;
    };
}
