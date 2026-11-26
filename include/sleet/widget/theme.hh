#pragma once
#include <optional>
#include <string>
#include <vector>

#include "sleet/color.hh"
#include "sleet/widget/style.hh"


namespace sleet::widget
{
    struct colors
    {
        std::optional<color> bg;
        std::optional<color> fg;


        void set_if_unset(const colors &other) noexcept
        {
            if (!bg) bg = other.bg;
            if (!fg) fg = other.fg;
        }
    };


    struct font_face
    {
        std::string path;
        float       size = 16.F;
    };


    struct font_config
    {
        font_face body;
        font_face heading;
        font_face caption;
        font_face code;

        /// Optional faces tried in order when the selected role lacks a glyph.
        /// The first face that covers the whole text run is used.
        std::vector<font_face> fallbacks;


        [[nodiscard]]
        auto face(font_role role) const noexcept -> const font_face &;
    };


    /// Checks common system font locations. If none are found, the paths stay
    /// empty and \c render_context reports the missing font when it starts.
    [[nodiscard]]
    auto default_font_config() -> font_config;


    struct theme
    {
        /// Colors for an action widget under the pointer.
        colors hover;

        /// Colors for an action widget while pressed.
        colors clicked;

        /// The default surface and text colors.
        colors base;

        /// The surface and text colors for text fields, combo boxes, and
        /// slider tracks.
        colors field;

        /// \c std::nullopt means no border.
        std::optional<color> border;

        /// The border color for the focused widget.
        std::optional<color> focus;

        /// The fill color for selected controls and primary actions.
        std::optional<color> accent;

        /// The text color drawn on an accent surface.
        std::optional<color> accent_fg;

        /// The fill color for destructive actions.
        std::optional<color> danger;

        /// The text color drawn on a danger surface.
        std::optional<color> danger_fg;

        /// The caret color in a text input.
        std::optional<color> caret;

        /// The background of selected text.
        std::optional<color> selection;

        /// The color for placeholders and secondary text.
        std::optional<color> muted;

        /// Colors used for disabled widgets.
        colors disabled;

        /// The border color for disabled widgets.
        std::optional<color> disabled_border;

        /// The default corner radius for interactive controls, in logical
        /// pixels. A widget can override it with \c style::corner_radius.
        float radius = 6.F;

        /// The radius used by small indicators such as checkboxes.
        float small_radius = 3.F;

        font_config fonts;


        [[nodiscard]] static auto default_dark() -> theme;
        [[nodiscard]] static auto default_light() -> theme;
    };
}
