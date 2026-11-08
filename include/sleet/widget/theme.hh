#pragma once
#include <optional>

#include <kei-utils/bitmask.hh>

#include "sleet/color.hh"


namespace sleet::widget
{
    struct colors
    {
        color bg;
        color fg;


        void set_if_unset(colors other)
        {
            if (bg.is_unset()) bg = other.bg;
            if (fg.is_unset()) fg = other.fg;
        }
    };


    struct theme
    {
        colors hover;
        colors clicked;
        colors base;

        /// `std::nullopt` means no border
        std::optional<color> border;


        [[nodiscard]] static auto default_dark() noexcept -> theme;
        [[nodiscard]] static auto default_light() noexcept -> theme;
    };
}
