#pragma once
#include <memory>
#include <optional>
#include <string_view>

#include <SDL3/SDL_render.h>

#include "sleet/math/vec2.hh"
#include "sleet/widget/control.hh"
#include "sleet/widget/layout.hh"
#include "sleet/widget/theme.hh"


namespace sleet
{
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
                          std::optional<float>     width_limit) -> kei::result<math::vec2>;

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

    private:
        struct impl;
        std::unique_ptr<impl> m_impl;
    };
}
