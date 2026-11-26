#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sleet/widget/theme.hh"


namespace
{
    using sleet::widget::font_face;


    auto first_existing(std::initializer_list<std::string_view> candidates) -> std::string
    {
        for (const auto path : candidates)
        {
            std::error_code error;
            if (std::filesystem::exists(path, error)) return std::string { path };
        }

        return {};
    }
}


auto sleet::widget::font_config::face(font_role role) const noexcept -> const font_face &
{
    const font_face *selected = &body;

    switch (role)
    {
    case font_role::body:    break;
    case font_role::heading: selected = &heading; break;
    case font_role::caption: selected = &caption; break;
    case font_role::code:    selected = &code; break;
    }

    // Missing roles inherit the body face, so a theme that sets only one font
    // still works for every role.
    if (selected->path.empty()) return body;
    return *selected;
}


auto sleet::widget::default_font_config() -> font_config
{
    const auto sans = first_existing({
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
    });

    const auto mono = first_existing({
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/noto/NotoSansMono-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/System/Library/Fonts/Menlo.ttc",
        "C:/Windows/Fonts/consola.ttf",
    });

    auto fallbacks = std::vector<font_face> {};
    for (const auto path : {
             "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
             "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
             "/usr/share/fonts/truetype/noto/NotoSansArabic-Regular.ttf",
             "/usr/share/fonts/truetype/noto/NotoSansHebrew-Regular.ttf",
             "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
             "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
             "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
             "C:/Windows/Fonts/segoeui.ttf",
         })
    {
        std::error_code error;
        if (!std::filesystem::exists(path, error)) continue;
        if (std::string { path } == sans) continue;
        fallbacks.push_back(font_face { path, 16.F });
    }

    return {
        .body      = font_face { sans,                       16.F },
        .heading   = font_face { sans,                       28.F },
        .caption   = font_face { sans,                       12.F },
        .code      = font_face { mono.empty() ? sans : mono, 14.F },
        .fallbacks = std::move(fallbacks),
    };
}


auto sleet::widget::theme::default_dark() -> theme
{
    using literals::operator""_rgb;

    theme result;
    result.base            = { .bg = 0x17191C_rgb, .fg = 0xE6E8EB_rgb };
    result.field           = { .bg = 0x1F2226_rgb, .fg = 0xE6E8EB_rgb };
    result.hover           = { .bg = 0x24272C_rgb, .fg = 0xFFFFFF_rgb };
    result.clicked         = { .bg = 0x2F333A_rgb, .fg = 0xFFFFFF_rgb };
    result.border          = 0x3A3F47_rgb;
    result.focus           = 0x5B9DFF_rgb;
    result.accent          = 0x3B82F6_rgb;
    result.accent_fg       = 0xFFFFFF_rgb;
    result.danger          = 0xDC4C4C_rgb;
    result.danger_fg       = 0xFFFFFF_rgb;
    result.caret           = 0xE6E8EB_rgb;
    result.selection       = 0x2F5FA8_rgb;
    result.muted           = 0x8A919B_rgb;
    result.scrollbar_track = 0x2A2E35_rgb;
    result.scrollbar_thumb = 0x5A626D_rgb;
    result.scrim           = color { 0, 0, 0, 150 };
    result.toast_bg        = 0x2A2E35_rgb;
    result.toast_fg        = 0xF3F5F8_rgb;
    result.disabled        = { .bg = 0x23262B_rgb, .fg = 0x6B7280_rgb };
    result.disabled_border = 0x33373D_rgb;
    result.radius          = 6.F;
    result.small_radius    = 3.F;
    result.fonts           = default_font_config();
    return result;
}


auto sleet::widget::theme::default_light() -> theme
{
    using literals::operator""_rgb;

    theme result;
    result.base            = { .bg = 0xF7F8FA_rgb, .fg = 0x1B1D21_rgb };
    result.field           = { .bg = 0xFFFFFF_rgb, .fg = 0x1B1D21_rgb };
    result.hover           = { .bg = 0xE9ECF0_rgb, .fg = 0x000000_rgb };
    result.clicked         = { .bg = 0xDCE1E7_rgb, .fg = 0x000000_rgb };
    result.border          = 0xC7CDD5_rgb;
    result.focus           = 0x2563EB_rgb;
    result.accent          = 0x2563EB_rgb;
    result.accent_fg       = 0xFFFFFF_rgb;
    result.danger          = 0xDC2626_rgb;
    result.danger_fg       = 0xFFFFFF_rgb;
    result.caret           = 0x1B1D21_rgb;
    result.selection       = 0xBFDBFE_rgb;
    result.muted           = 0x6B7280_rgb;
    result.scrollbar_track = 0xE5E8EC_rgb;
    result.scrollbar_thumb = 0xB6BDC6_rgb;
    result.scrim           = color { 0, 0, 0, 90 };
    result.toast_bg        = 0x1B1D21_rgb;
    result.toast_fg        = 0xF3F5F8_rgb;
    result.disabled        = { .bg = 0xEDEFF2_rgb, .fg = 0x9CA3AF_rgb };
    result.disabled_border = 0xD1D5DB_rgb;
    result.radius          = 6.F;
    result.small_radius    = 3.F;
    result.fonts           = default_font_config();
    return result;
}
