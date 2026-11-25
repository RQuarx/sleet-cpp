#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SDL3_ttf/SDL_ttf.h>

#include "sleet/render_context.hh"
#include "sleet/text/wrap.hh"


namespace
{
    using sleet::color;
    using sleet::math::rect;
    using sleet::math::vec2;
    using sleet::widget::colors;
    using sleet::widget::font_role;
    using sleet::widget::font_spec;
    using sleet::widget::node_data;


    [[nodiscard]]
    auto merge_colors(const colors &over, const colors &under) noexcept -> colors
    {
        return {
            .bg = over.bg ? over.bg : under.bg,
            .fg = over.fg ? over.fg : under.fg,
        };
    }


    [[nodiscard]]
    auto to_sdl_rect(const rect &value) noexcept -> SDL_Rect
    {
        return {
            .x = static_cast<int>(std::floor(value.position.x)),
            .y = static_cast<int>(std::floor(value.position.y)),
            .w = std::max(0, static_cast<int>(std::ceil(value.size.x))),
            .h = std::max(0, static_cast<int>(std::ceil(value.size.y))),
        };
    }


    [[nodiscard]]
    auto to_frect(const rect &value) noexcept -> SDL_FRect
    { return { value.position.x, value.position.y, value.size.x, value.size.y }; }


    [[nodiscard]]
    auto intersect(const SDL_Rect &lhs, const SDL_Rect &rhs) noexcept -> SDL_Rect
    {
        const int x1 = std::max(lhs.x, rhs.x);
        const int y1 = std::max(lhs.y, rhs.y);
        const int x2 = std::min(lhs.x + lhs.w, rhs.x + rhs.w);
        const int y2 = std::min(lhs.y + lhs.h, rhs.y + rhs.h);

        if (x2 <= x1 or y2 <= y1) return { .x = x1, .y = y1, .w = 0, .h = 0 };
        return { .x = x1, .y = y1, .w = x2 - x1, .h = y2 - y1 };
    }


    [[nodiscard]]
    auto align_offset(sleet::widget::align alignment, float free_space) noexcept -> float
    {
        const auto free = std::max(0.F, free_space);

        switch (alignment)
        {
        case sleet::widget::align::start:  return 0.F;
        case sleet::widget::align::center: return free / 2.F;
        case sleet::widget::align::end:    return free;
        }

        return 0.F;
    }


    struct font_key
    {
        font_role          role  = font_role::body;
        int                size  = 16;
        TTF_FontStyleFlags flags = TTF_STYLE_NORMAL;

        [[nodiscard]] friend auto operator==(font_key, font_key) noexcept -> bool = default;
    };


    struct font_key_hash
    {
        [[nodiscard]]
        auto operator()(const font_key &key) const noexcept -> std::size_t
        {
            auto seed = std::hash<int> {}(static_cast<int>(key.role));
            seed     ^= std::hash<int> {}(key.size) + 0x9E3779B9U + (seed << 6U) + (seed >> 2U);
            seed     ^= std::hash<TTF_FontStyleFlags> {}(key.flags) + 0x9E3779B9U + (seed << 6U)
                      + (seed >> 2U);
            return seed;
        }
    };


    struct text_key
    {
        std::string text;
        TTF_Font   *font = nullptr;
        color       fg {};

        [[nodiscard]] friend auto operator==(const text_key &lhs, const text_key &rhs) noexcept
            -> bool
        { return lhs.text == rhs.text and lhs.font == rhs.font and lhs.fg == rhs.fg; }
    };


    struct text_key_hash
    {
        [[nodiscard]]
        auto operator()(const text_key &key) const noexcept -> std::size_t
        {
            auto seed = std::hash<std::string> {}(key.text);
            seed
                ^= std::hash<const void *> {}(key.font) + 0x9E3779B9U + (seed << 6U) + (seed >> 2U);

            const auto packed = (static_cast<std::uint32_t>(key.fg.r) << 24U)
                              | (static_cast<std::uint32_t>(key.fg.g) << 16U)
                              | (static_cast<std::uint32_t>(key.fg.b) << 8U)
                              | static_cast<std::uint32_t>(key.fg.a);
            seed ^= std::hash<std::uint32_t> {}(packed) + 0x9E3779B9U + (seed << 6U) + (seed >> 2U);
            return seed;
        }
    };


    struct text_entry
    {
        SDL_Texture *texture = nullptr;
        vec2         size {};
    };


    struct text_clip
    {
        bool     had_clip = false;
        SDL_Rect previous {};
    };
}


struct sleet::render_context::impl
{
    SDL_Renderer *renderer = nullptr;
    widget::theme theme;
    vec2          viewport { 0.F, 0.F };

    std::optional<widget::widget_id> hovered;
    std::optional<widget::widget_id> pressed;
    std::optional<widget::widget_id> focused;
    std::optional<widget::widget_id> editing;
    widget::text_selection           selection;

    std::unordered_map<font_key, TTF_Font *, font_key_hash> fonts;
    std::unordered_map<text_key, text_entry, text_key_hash> text_cache;


    impl(SDL_Renderer *renderer, widget::theme theme)
        : renderer { renderer }, theme { std::move(theme) }
    {
    }

    ~impl()
    {
        clear_text_cache();
        clear_fonts();
    }


    void clear_text_cache() noexcept
    {
        for (auto &[key, entry] : text_cache)
            if (entry.texture != nullptr) SDL_DestroyTexture(entry.texture);
        text_cache.clear();
    }


    void clear_fonts() noexcept
    {
        for (auto &[key, font] : fonts)
            if (font != nullptr) TTF_CloseFont(font);
        fonts.clear();
    }


    [[nodiscard]]
    auto apply_viewport_clip() -> kei::result<>
    {
        if (!std::isfinite(viewport.x) or !std::isfinite(viewport.y) or viewport.x <= 0.F
            or viewport.y <= 0.F)
        {
            if (!SDL_SetRenderClipRect(renderer, nullptr))
                return sleet::sdl_error { "Failed to clear the viewport clip." }.unexpected();
            return {};
        }

        const SDL_Rect clip {
            .x = 0,
            .y = 0,
            .w = std::max(0, static_cast<int>(std::ceil(viewport.x))),
            .h = std::max(0, static_cast<int>(std::ceil(viewport.y))),
        };
        if (!SDL_SetRenderClipRect(renderer, &clip))
            return sleet::sdl_error { "Failed to set the viewport clip." }.unexpected();
        return {};
    }


    [[nodiscard]]
    auto state_matches(const node_data                        &node,
                       const std::optional<widget::widget_id> &id) const noexcept -> bool
    {
        for (const auto *current = &node; current != nullptr; current = current->parent)
            if (id and *id == current->id) return true;
        return false;
    }


    [[nodiscard]]
    auto is_hovered(const node_data &node) const noexcept -> bool
    { return state_matches(node, hovered); }


    [[nodiscard]]
    auto is_pressed(const node_data &node) const noexcept -> bool
    { return state_matches(node, pressed); }


    [[nodiscard]]
    auto theme_color(widget::tone value) const noexcept -> std::optional<color>
    {
        switch (value)
        {
        case widget::tone::none:      return std::nullopt;
        case widget::tone::base:      return theme.base.bg;
        case widget::tone::field:     return theme.field.bg;
        case widget::tone::muted:     return theme.muted;
        case widget::tone::accent:    return theme.accent;
        case widget::tone::accent_fg: return theme.accent_fg;
        case widget::tone::danger:    return theme.danger;
        case widget::tone::danger_fg: return theme.danger_fg;
        case widget::tone::border:    return theme.border;
        }

        return std::nullopt;
    }


    [[nodiscard]]
    auto active_colors(const node_data &node) const noexcept -> colors
    {
        auto result = theme.base;

        if (is_hovered(node)) result = merge_colors(theme.hover, result);
        if (is_pressed(node)) result = merge_colors(theme.clicked, result);

        return result;
    }


    [[nodiscard]]
    auto get_font(const font_spec &spec) -> kei::result<TTF_Font *>
    {
        const auto &face = theme.fonts.face(spec.role);
        if (face.path.empty())
            return sleet::error { "No font configured for the {} role.",
                                  static_cast<int>(spec.role) }
                .unexpected();

        const auto requested = spec.size.value_or(face.size);
        const auto size      = std::clamp(std::isfinite(requested) ? requested : 16.F, 1.F, 1024.F);
        TTF_FontStyleFlags flags = TTF_STYLE_NORMAL;

        if (spec.weight and *spec.weight >= widget::font_weight::semibold) flags |= TTF_STYLE_BOLD;
        if (spec.attribute.has(widget::text_attribute::italic)) flags |= TTF_STYLE_ITALIC;
        if (spec.attribute.has(widget::text_attribute::underline)) flags |= TTF_STYLE_UNDERLINE;
        if (spec.attribute.has(widget::text_attribute::strikethrough))
            flags |= TTF_STYLE_STRIKETHROUGH;

        const font_key key {
            .role  = spec.role,
            .size  = static_cast<int>(std::lround(size * 100.F)),
            .flags = flags,
        };

        if (const auto it = fonts.find(key); it != fonts.end()) return it->second;

        auto *font = TTF_OpenFont(face.path.c_str(), size);
        if (font == nullptr)
            return sleet::sdl_error { "Failed to open font '{}' at {} px", face.path, size }
                .unexpected();

        TTF_SetFontStyle(font, flags);
        fonts.emplace(key, font);
        return font;
    }


    [[nodiscard]]
    auto measure_line(TTF_Font *font, std::string_view line) -> kei::result<float>
    {
        if (line.empty()) return 0.F;

        int width  = 0;
        int height = 0;
        if (!TTF_GetStringSize(font, line.data(), line.size(), &width, &height))
            return sleet::sdl_error { "Failed to measure text." }.unexpected();

        return static_cast<float>(width);
    }


    [[nodiscard]]
    auto wrapped_lines(TTF_Font *font, std::string_view text, std::optional<float> width_limit)
        -> kei::result<std::vector<sleet::txt::wrapped_line>>
    {
        const auto measure
            = [this, font](std::string_view line) { return measure_line(font, line); };
        return sleet::txt::wrap(text, width_limit, measure);
    }


    [[nodiscard]]
    auto get_text_texture(std::string_view text, TTF_Font *font, color fg)
        -> kei::result<text_entry *>
    {
        text_key key { .text = std::string { text }, .font = font, .fg = fg };

        if (const auto it = text_cache.find(key); it != text_cache.end()) return &it->second;

        if (text_cache.size() > 2048) clear_text_cache();

        auto *surface = TTF_RenderText_Blended(font, text.data(), text.size(), fg.to_sdl());
        if (surface == nullptr) return sleet::sdl_error { "Failed to render text." }.unexpected();

        auto *texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_DestroySurface(surface);

        if (texture == nullptr)
            return sleet::sdl_error { "Failed to create a text texture." }.unexpected();

        float width  = 0.F;
        float height = 0.F;
        if (!SDL_GetTextureSize(texture, &width, &height))
        {
            SDL_DestroyTexture(texture);
            return sleet::sdl_error { "Failed to query the text texture size." }.unexpected();
        }

        auto [it, inserted] = text_cache.emplace(
            std::move(key), text_entry {
                                .texture = texture, .size = { width, height }
        });
        return &it->second;
    }


    [[nodiscard]]
    auto fill_rect(const rect &value, color fill) -> kei::result<>
    {
        if (!SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a))
            return sleet::sdl_error { "Failed to set the fill color." }.unexpected();

        const auto sdl_rect = to_frect(value);
        if (!SDL_RenderFillRect(renderer, &sdl_rect))
            return sleet::sdl_error { "Failed to fill a rectangle." }.unexpected();

        return {};
    }


    [[nodiscard]]
    auto draw_border(const rect &value, color border, float width) -> kei::result<>
    {
        const auto requested = std::isfinite(width) ? std::max(0.F, width) : 0.F;
        const auto thickness = std::min({ requested, value.size.x / 2.F, value.size.y / 2.F });
        if (thickness <= 0.F) return {};

        if (!SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a))
            return sleet::sdl_error { "Failed to set the border color." }.unexpected();

        const SDL_FRect edges[] {
            { value.position.x, value.position.y, value.size.x, thickness },
            { value.position.x, value.position.y + value.size.y - thickness, value.size.x,
             thickness },
            { value.position.x, value.position.y + thickness, thickness,
             std::max(0.F, value.size.y - (thickness * 2.F)) },
            { value.position.x + value.size.x - thickness, value.position.y + thickness, thickness,
             std::max(0.F, value.size.y - (thickness * 2.F)) },
        };

        for (const auto &edge : edges)
            if (!SDL_RenderFillRect(renderer, &edge))
                return sleet::sdl_error { "Failed to draw a border edge." }.unexpected();

        return {};
    }


    [[nodiscard]]
    auto inherited_node_foreground(const node_data &node) const noexcept -> std::optional<color>
    {
        switch (node.variant)
        {
        case widget::button_variant::primary:  return theme.accent_fg;
        case widget::button_variant::danger:   return theme.danger_fg;
        case widget::button_variant::standard:
        case widget::button_variant::ghost:    break;
        }

        switch (node.tint)
        {
        case widget::tone::accent:    return theme.accent_fg;
        case widget::tone::danger:    return theme.danger_fg;
        case widget::tone::accent_fg: return theme.accent_fg;
        case widget::tone::danger_fg: return theme.danger_fg;
        case widget::tone::muted:     return theme.muted;
        case widget::tone::none:
        case widget::tone::base:
        case widget::tone::field:
        case widget::tone::border:    break;
        }

        return std::nullopt;
    }


    [[nodiscard]]
    auto inherited_foreground(const node_data &node) const noexcept -> std::optional<color>
    {
        for (const auto *current = &node; current != nullptr; current = current->parent)
            if (const auto inherited = inherited_node_foreground(*current)) return inherited;

        return std::nullopt;
    }


    [[nodiscard]]
    auto foreground_for(const node_data &node, const colors &active) const noexcept -> color
    {
        if (node.visual.foreground) return *node.visual.foreground;
        if (const auto inherited = inherited_foreground(node)) return *inherited;

        if (node.control == widget::control_kind::text_input
            or node.control == widget::control_kind::combo_box)
            if (theme.field.fg) return *theme.field.fg;

        return active.fg.value_or(color { 255, 255, 255, 255 });
    }


    [[nodiscard]]
    auto variant_background(const node_data &node) const noexcept -> std::optional<color>
    {
        switch (node.variant)
        {
        case widget::button_variant::primary:  return theme.accent;
        case widget::button_variant::danger:   return theme.danger;
        case widget::button_variant::ghost:
        case widget::button_variant::standard: break;
        }

        return std::nullopt;
    }


    [[nodiscard]]
    auto control_background(const node_data &node) const noexcept -> std::optional<color>
    {
        switch (node.control)
        {
        case widget::control_kind::text_input:
        case widget::control_kind::combo_box:
            if (is_hovered(node) or is_pressed(node))
                return theme.hover.bg ? theme.hover.bg : theme.field.bg;
            return theme.field.bg;
        case widget::control_kind::combo_option:
            if (is_pressed(node)) return theme.clicked.bg;
            if (is_hovered(node)) return theme.hover.bg;
            return std::nullopt;
        case widget::control_kind::checkbox_indicator:
        case widget::control_kind::radio_indicator:
            return node.checked ? theme.accent : theme.field.bg;
        case widget::control_kind::slider:
        case widget::control_kind::none:   break;
        }

        return std::nullopt;
    }


    [[nodiscard]]
    auto background_for(const node_data &node, const colors &active) const noexcept
        -> std::optional<color>
    {
        if (node.visual.background) return node.visual.background;

        // A ghost button stays transparent except while hovered or pressed.
        if (node.variant == widget::button_variant::ghost)
        {
            if (is_pressed(node)) return theme.clicked.bg;
            if (is_hovered(node)) return theme.hover.bg;
            return std::nullopt;
        }

        if (const auto variant = variant_background(node)) return variant;
        if (const auto control = control_background(node)) return control;
        if (const auto tinted = theme_color(node.tint)) return tinted;

        if (!node.action.empty() or node.focusable) return active.bg;
        return std::nullopt;
    }


    [[nodiscard]]
    auto border_for(const node_data &node) const noexcept -> std::optional<color>
    {
        if (node.visual.border) return node.visual.border;

        if (focused and *focused == node.id) return theme.focus;

        switch (node.control)
        {
        case widget::control_kind::text_input:
        case widget::control_kind::combo_box:          return theme.border;
        case widget::control_kind::checkbox_indicator:
        case widget::control_kind::radio_indicator:
            return node.checked ? theme.accent : theme.border;
        case widget::control_kind::combo_option:
        case widget::control_kind::slider:
        case widget::control_kind::none:         break;
        }

        if (node.variant == widget::button_variant::ghost) return std::nullopt;
        if (!node.action.empty()) return theme.border;
        return std::nullopt;
    }


    [[nodiscard]]
    auto border_width_for(const node_data &node) const noexcept -> float
    {
        if (focused and *focused == node.id) return 2.F;
        return node.visual.border_width;
    }


    [[nodiscard]]
    auto draw_text_frame(const node_data &node, const rect &bounds, const colors &active)
        -> kei::result<>
    {
        if (const auto background = background_for(node, active))
            if (auto res = fill_rect(bounds, *background); !res) return res;

        if (const auto border = border_for(node))
            if (auto res = draw_border(bounds, *border, border_width_for(node)); !res) return res;

        return {};
    }


    [[nodiscard]]
    auto enter_text_clip(const rect &content) -> kei::result<std::optional<text_clip>>
    {
        text_clip state;
        state.had_clip = SDL_RenderClipEnabled(renderer);

        if (state.had_clip and !SDL_GetRenderClipRect(renderer, &state.previous))
            return sleet::sdl_error { "Failed to query the current clip rectangle." }.unexpected();

        auto clip = to_sdl_rect(content);
        if (state.had_clip)
        {
            clip = intersect(clip, state.previous);

            // SDL treats an empty clip rectangle as "clipping disabled". If the
            // text falls completely outside the current clip, skip it rather
            // than let it draw over the rest of the frame.
            if (clip.w <= 0 or clip.h <= 0) return std::optional<text_clip> {};
        }

        if (!SDL_SetRenderClipRect(renderer, &clip))
            return sleet::sdl_error { "Failed to set the text clip rectangle." }.unexpected();

        return std::optional<text_clip> { state };
    }


    [[nodiscard]]
    auto leave_text_clip(const text_clip &state) -> kei::result<>
    {
        if (!SDL_SetRenderClipRect(renderer, state.had_clip ? &state.previous : nullptr))
            return sleet::sdl_error { "Failed to restore the text clip rectangle." }.unexpected();
        return {};
    }


    [[nodiscard]]
    auto draw_text_line(const node_data         &node,
                        const rect              &content,
                        TTF_Font                *font,
                        color                    fg,
                        const txt::wrapped_line &line,
                        std::size_t              index,
                        float                    line_height,
                        float                    y_offset) -> kei::result<>
    {
        if (line.text.empty()) return {};

        auto entry = get_text_texture(line.text, font, fg);
        if (!entry) return std::move(entry.error()).unexpected();

        const auto      x_offset = align_offset(node.alignment.x, content.size.x - line.width);
        const SDL_FRect destination {
            content.position.x + x_offset,
            content.position.y + y_offset + (line_height * static_cast<float>(index))
                + ((line_height - (*entry)->size.y) / 2.F),
            (*entry)->size.x,
            (*entry)->size.y,
        };

        if (!SDL_RenderTexture(renderer, (*entry)->texture, nullptr, &destination))
            return sleet::sdl_error { "Failed to draw text." }.unexpected();

        return {};
    }


    [[nodiscard]]
    auto draw_text_lines(const node_data                      &node,
                         const rect                           &content,
                         TTF_Font                             *font,
                         color                                 fg,
                         const std::vector<txt::wrapped_line> &lines) -> kei::result<>
    {
        const auto line_height  = static_cast<float>(std::max(1, TTF_GetFontHeight(font)));
        const auto total_height = line_height * static_cast<float>(lines.size());
        const auto y_offset     = align_offset(node.alignment.y, content.size.y - total_height);

        for (std::size_t i = 0; i < lines.size(); i++)
            if (auto res
                = draw_text_line(node, content, font, fg, lines[i], i, line_height, y_offset);
                !res)
                return res;

        return {};
    }


    struct text_input_view
    {
        float scroll  = 0.F;
        float caret_x = 0.F;
    };


    [[nodiscard]]
    auto text_input_caret(TTF_Font *font, std::string_view text, float available_width)
        -> kei::result<text_input_view>
    {
        const auto offset = widget::clamp_offset(text, selection.caret);
        auto       width  = measure_line(font, text.substr(0, offset));
        if (!width) return std::move(width.error()).unexpected();

        text_input_view view { .caret_x = *width };
        const auto      overflow = view.caret_x - available_width + 2.F;
        if (overflow > 0.F) view.scroll = overflow;
        return view;
    }


    [[nodiscard]]
    auto text_selection_span(TTF_Font *font, std::string_view text)
        -> kei::result<std::pair<float, float>>
    {
        const auto first = widget::clamp_offset(text, selection.begin());
        const auto last  = widget::clamp_offset(text, selection.end());

        auto first_width = measure_line(font, text.substr(0, first));
        if (!first_width) return std::move(first_width.error()).unexpected();

        auto last_width = measure_line(font, text.substr(0, last));
        if (!last_width) return std::move(last_width.error()).unexpected();

        return std::pair { std::min(*first_width, *last_width),
                           std::max(*first_width, *last_width) };
    }


    [[nodiscard]]
    auto
    draw_text_selection(const rect &content, TTF_Font *font, std::string_view text, float scroll)
        -> kei::result<>
    {
        if (!selection.has_selection() or text.empty()) return {};

        auto span = text_selection_span(font, text);
        if (!span) return std::move(span.error()).unexpected();

        const auto [x1, x2] = *span;
        const rect highlight {
            content.position.x + x1 - scroll,
            content.position.y + 2.F,
            x2 - x1,
            std::max(0.F, content.size.y - 4.F),
        };

        if (highlight.size.x <= 0.F) return {};
        return fill_rect(highlight, theme.selection.value_or(color { 47, 95, 168, 255 }));
    }


    [[nodiscard]]
    auto draw_text_input(const node_data &node, const rect &bounds, TTF_Font *font, color fg)
        -> kei::result<>
    {
        const auto content = bounds.inset(node.padding);
        if (content.size.x <= 0.F or content.size.y <= 0.F) return {};

        const bool is_editing = editing and *editing == node.id;
        const auto text       = std::string_view { node.text };
        const auto display    = text.empty() ? std::string_view { node.placeholder } : text;
        const auto text_color = text.empty() ? theme.muted.value_or(fg) : fg;

        auto clip = enter_text_clip(content);
        if (!clip) return std::move(clip.error()).unexpected();
        if (!*clip) return {};

        const auto restore_after = [this, &clip](kei::result<> result) -> kei::result<>
        {
            auto restored = leave_text_clip(**clip);
            if (!result) return result;
            return restored;
        };

        auto view = text_input_view {};
        if (is_editing)
        {
            auto computed = text_input_caret(font, text, content.size.x);
            if (!computed) return restore_after(std::move(computed.error()).unexpected());
            view = *computed;
        }

        if (is_editing)
            if (auto res = draw_text_selection(content, font, text, view.scroll); !res)
                return restore_after(std::move(res));

        if (!display.empty())
        {
            auto entry = get_text_texture(display, font, text_color);
            if (!entry) return restore_after(std::move(entry.error()).unexpected());

            const SDL_FRect destination {
                content.position.x - view.scroll,
                content.position.y + ((content.size.y - (*entry)->size.y) / 2.F),
                (*entry)->size.x,
                (*entry)->size.y,
            };

            if (!SDL_RenderTexture(renderer, (*entry)->texture, nullptr, &destination))
                return restore_after(sleet::sdl_error { "Failed to draw text." }.unexpected());
        }

        if (is_editing)
        {
            const rect caret_rect {
                content.position.x + view.caret_x - view.scroll,
                content.position.y + 2.F,
                2.F,
                std::max(0.F, content.size.y - 4.F),
            };

            if (auto res = fill_rect(caret_rect, theme.caret.value_or(fg)); !res)
                return restore_after(std::move(res));
        }

        return restore_after({});
    }


    [[nodiscard]]
    auto draw_text(const node_data &node, const rect &bounds) -> kei::result<>
    {
        auto font = get_font(node.font);
        if (!font) return std::move(font.error()).unexpected();

        const auto content = bounds.inset(node.padding);
        const auto active  = active_colors(node);
        const auto fg      = foreground_for(node, active);

        if (auto res = draw_text_frame(node, bounds, active); !res) return res;

        if (node.control == widget::control_kind::text_input)
            return draw_text_input(node, bounds, *font, fg);

        if (content.size.x <= 0.F or content.size.y <= 0.F) return {};

        auto lines = wrapped_lines(*font, node.text, content.size.x);
        if (!lines) return std::move(lines.error()).unexpected();
        if (lines->empty()) return {};

        auto clip = enter_text_clip(content);
        if (!clip) return std::move(clip.error()).unexpected();
        if (!*clip) return {};

        auto drawn    = draw_text_lines(node, content, *font, fg, *lines);
        auto restored = leave_text_clip(**clip);

        if (!drawn) return drawn;
        if (!restored) return restored;
        return {};
    }


    [[nodiscard]]
    auto draw_check(const rect &bounds, color stroke) -> kei::result<>
    {
        if (!SDL_SetRenderDrawColor(renderer, stroke.r, stroke.g, stroke.b, stroke.a))
            return sleet::sdl_error { "Failed to set the indicator color." }.unexpected();

        const auto x = bounds.position.x;
        const auto y = bounds.position.y;
        const auto w = bounds.size.x;
        const auto h = bounds.size.y;

        if (!SDL_RenderLine(renderer, x + (w * 0.20F), y + (h * 0.52F), x + (w * 0.42F),
                            y + (h * 0.75F)))
            return sleet::sdl_error { "Failed to draw a check mark." }.unexpected();
        if (!SDL_RenderLine(renderer, x + (w * 0.42F), y + (h * 0.75F), x + (w * 0.82F),
                            y + (h * 0.26F)))
            return sleet::sdl_error { "Failed to draw a check mark." }.unexpected();

        return {};
    }


    [[nodiscard]]
    auto draw_indicator(const node_data &node, const rect &bounds) -> kei::result<>
    {
        if (!node.checked) return {};

        const auto stroke
            = theme.accent_fg.value_or(theme.base.fg.value_or(color { 255, 255, 255, 255 }));

        switch (node.control)
        {
        case widget::control_kind::checkbox_indicator:
            return draw_check(bounds.inset(sleet::math::edge_insets { 3.F }), stroke);
        case widget::control_kind::radio_indicator:
            return fill_rect(bounds.inset(sleet::math::edge_insets { 5.F }), stroke);
        case widget::control_kind::none:
        case widget::control_kind::text_input:
        case widget::control_kind::combo_box:
        case widget::control_kind::combo_option:
        case widget::control_kind::slider:       return {};
        }

        return {};
    }


    [[nodiscard]]
    auto draw_node(const node_data &node, const widget::layout_result &layout) -> kei::result<>
    {
        if (!node.visible) return {};

        const auto *bounds = layout.find(node.id);
        if (bounds == nullptr)
            return sleet::error { "No layout rectangle for widget \"{}\".", node.id.view() }
                .unexpected();

        if (node.is_text()) return draw_text(node, *bounds);

        const auto active = active_colors(node);

        if (const auto background = background_for(node, active))
            if (auto res = fill_rect(*bounds, *background); !res) return res;

        if (const auto border = border_for(node))
            if (auto res = draw_border(*bounds, *border, border_width_for(node)); !res) return res;

        if (node.control == widget::control_kind::checkbox_indicator
            or node.control == widget::control_kind::radio_indicator)
            if (auto res = draw_indicator(node, *bounds); !res) return res;

        for (const auto &child : node.children)
            if (auto result = draw_node(child, layout); !result)
                return std::move(result.error()).unexpected();

        return {};
    }
};


sleet::render_context::render_context(SDL_Renderer *renderer, widget::theme theme)
    : m_impl { std::make_unique<impl>(renderer, std::move(theme)) }
{
    if (renderer == nullptr) throw kei::error { "render_context requires a renderer." };

    if (!SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND))
        throw sleet::sdl_error { "Failed to enable alpha blending." };

    auto body_font = m_impl->get_font(widget::font_spec {});
    if (!body_font) throw std::move(body_font.error());
}


sleet::render_context::~render_context()                                              = default;
sleet::render_context::render_context(render_context &&) noexcept                     = default;
auto sleet::render_context::operator=(render_context &&) noexcept -> render_context & = default;


auto sleet::render_context::measure_text(std::string_view         text,
                                         const widget::font_spec &font,
                                         std::optional<float>     width_limit)
    -> kei::result<math::vec2>
{
    auto ttf_font = m_impl->get_font(font);
    if (!ttf_font) return std::move(ttf_font.error()).unexpected();

    if (text.empty()) return vec2 { 0.F, 0.F };

    auto lines = m_impl->wrapped_lines(*ttf_font, text, width_limit);
    if (!lines) return std::move(lines.error()).unexpected();
    if (lines->empty()) return vec2 { 0.F, 0.F };

    float width = 0.F;
    for (const auto &line : *lines) width = std::max(width, line.width);

    const auto line_height = static_cast<float>(std::max(1, TTF_GetFontHeight(*ttf_font)));
    return vec2 { width, line_height * static_cast<float>(lines->size()) };
}


auto sleet::render_context::render(const widget::node_data     &root,
                                   const widget::layout_result &layout) -> kei::result<>
{
    const auto background = m_impl->theme.base.bg.value_or(color { 0, 0, 0, 255 });
    if (!SDL_SetRenderDrawColor(m_impl->renderer, background.r, background.g, background.b,
                                background.a))
        return sleet::sdl_error { "Failed to set the clear color." }.unexpected();

    if (!SDL_RenderClear(m_impl->renderer))
        return sleet::sdl_error { "Failed to clear the renderer." }.unexpected();

    if (auto res = m_impl->apply_viewport_clip(); !res) return res;

    auto       result  = m_impl->draw_node(root, layout);
    const bool cleared = SDL_SetRenderClipRect(m_impl->renderer, nullptr);

    if (!result) return result;
    if (!cleared)
        return sleet::sdl_error { "Failed to clear the render clip rectangle." }.unexpected();

    return {};
}


void sleet::render_context::set_theme(widget::theme theme)
{
    auto previous = std::move(m_impl->theme);

    m_impl->clear_text_cache();
    m_impl->clear_fonts();
    m_impl->theme = std::move(theme);

    // Validate the new theme before the next frame. If the body face cannot be
    // opened, restore the old theme so the context remains usable.
    if (auto body_font = m_impl->get_font(widget::font_spec {}); !body_font)
    {
        m_impl->clear_text_cache();
        m_impl->clear_fonts();
        m_impl->theme = std::move(previous);
        throw std::move(body_font.error());
    }
}

void sleet::render_context::set_hovered(std::optional<widget::widget_id> id) noexcept
{ m_impl->hovered = std::move(id); }

void sleet::render_context::set_pressed(std::optional<widget::widget_id> id) noexcept
{ m_impl->pressed = std::move(id); }

void sleet::render_context::set_focused(std::optional<widget::widget_id> id) noexcept
{ m_impl->focused = std::move(id); }

void sleet::render_context::set_text_selection(std::optional<widget::widget_id> id,
                                               widget::text_selection           selection) noexcept
{
    m_impl->editing   = std::move(id);
    m_impl->selection = selection;
}

void sleet::render_context::set_viewport(math::vec2 viewport) noexcept
{ m_impl->viewport = viewport; }
