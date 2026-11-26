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
#include <utf8.h>

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
        int                face  = 0;
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
            seed     ^= std::hash<int> {}(key.face) + 0x9E3779B9U + (seed << 6U) + (seed >> 2U);
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
        std::size_t  last_used = 0;
    };


    [[nodiscard]]
    constexpr auto mix(color first, color second, float amount) noexcept -> color
    {
        const auto t     = std::clamp(amount, 0.F, 1.F);
        const auto blend = [t](std::uint8_t a, std::uint8_t b)
        {
            return static_cast<std::uint8_t>(std::lround(
                static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t));
        };

        return { blend(first.r, second.r), blend(first.g, second.g), blend(first.b, second.b),
                 blend(first.a, second.a) };
    }


    struct text_clip
    {
        bool     had_clip = false;
        SDL_Rect previous {};
    };


    [[nodiscard]]
    auto vertex(SDL_FPoint point, color value) noexcept -> SDL_Vertex
    {
        return {
            .position = point,
            .color = { static_cast<float>(value.r) / 255.F, static_cast<float>(value.g) / 255.F,
                      static_cast<float>(value.b) / 255.F, static_cast<float>(value.a) / 255.F },
            .tex_coord = { 0.F, 0.F },
        };
    }


    /// Builds a clockwise rounded-rectangle contour. \p inflate expands the
    /// rectangle and its radius; negative values shrink both.
    [[nodiscard]]
    auto rounded_contour(const rect &bounds, float radius, float inflate) -> std::vector<SDL_FPoint>
    {
        const auto x = bounds.position.x - inflate;
        const auto y = bounds.position.y - inflate;
        const auto w = std::max(0.F, bounds.size.x + (inflate * 2.F));
        const auto h = std::max(0.F, bounds.size.y + (inflate * 2.F));
        const auto r = std::clamp(radius + inflate, 0.F, std::min(w, h) / 2.F);

        constexpr auto segments = 8;
        constexpr auto pi       = 3.14159265358979323846F;
        constexpr auto half_pi  = pi / 2.F;

        std::vector<SDL_FPoint> points;
        points.reserve(4 * (segments + 1));

        const auto arc = [&](float cx, float cy, float start, float end)
        {
            for (int i = 0; i <= segments; i++)
            {
                const auto angle
                    = start
                    + ((end - start) * static_cast<float>(i) / static_cast<float>(segments));
                points.push_back({ cx + (r * std::cos(angle)), cy + (r * std::sin(angle)) });
            }
        };

        // Screen coordinates point down, so this walks clockwise starting at
        // the left edge.
        arc(x + r, y + r, pi, pi + half_pi);
        arc(x + w - r, y + r, pi + half_pi, 2.F * pi);
        arc(x + w - r, y + h - r, 0.F, half_pi);
        arc(x + r, y + h - r, half_pi, pi);
        return points;
    }


    [[nodiscard]]
    auto render_geometry(SDL_Renderer *renderer, const std::vector<SDL_Vertex> &vertices)
        -> kei::result<>
    {
        if (vertices.empty()) return {};
        if (!SDL_RenderGeometry(renderer, nullptr, vertices.data(),
                                static_cast<int>(vertices.size()), nullptr, 0))
            return sleet::sdl_error { "Failed to draw a rounded shape." }.unexpected();
        return {};
    }


    void emit_triangle(std::vector<SDL_Vertex> &vertices,
                       SDL_FPoint               a,
                       SDL_FPoint               b,
                       SDL_FPoint               c,
                       color                    ca,
                       color                    cb,
                       color                    cc)
    {
        vertices.push_back(vertex(a, ca));
        vertices.push_back(vertex(b, cb));
        vertices.push_back(vertex(c, cc));
    }


    void emit_strip(std::vector<SDL_Vertex>       &vertices,
                    const std::vector<SDL_FPoint> &inner,
                    const std::vector<SDL_FPoint> &outer,
                    color                          inner_color,
                    color                          outer_color)
    {
        const auto count = std::min(inner.size(), outer.size());
        if (count < 2) return;

        for (std::size_t i = 0; i < count; i++)
        {
            const auto next = (i + 1) % count;
            emit_triangle(vertices, inner[i], outer[i], outer[next], inner_color, outer_color,
                          outer_color);
            emit_triangle(vertices, inner[i], outer[next], inner[next], inner_color, outer_color,
                          inner_color);
        }
    }


    [[nodiscard]]
    auto fill_raw(SDL_Renderer *renderer, const rect &bounds, color fill) -> kei::result<>
    {
        if (!SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a))
            return sleet::sdl_error { "Failed to set the fill color." }.unexpected();

        const auto sdl_rect = to_frect(bounds);
        if (!SDL_RenderFillRect(renderer, &sdl_rect))
            return sleet::sdl_error { "Failed to fill a rectangle." }.unexpected();
        return {};
    }


    [[nodiscard]]
    auto fill_rounded(SDL_Renderer *renderer, const rect &bounds, float radius, color fill)
        -> kei::result<>
    {
        const auto limit = std::min(bounds.size.x, bounds.size.y) / 2.F;
        radius           = std::clamp(radius, 0.F, std::max(0.F, limit));

        if (radius <= 0.5F) return fill_raw(renderer, bounds, fill);
        if (bounds.size.x <= 0.F or bounds.size.y <= 0.F) return {};

        const auto solid  = rounded_contour(bounds, radius, -0.5F);
        const auto outer  = rounded_contour(bounds, radius, 0.5F);
        const auto center = SDL_FPoint { bounds.center().x, bounds.center().y };

        std::vector<SDL_Vertex> vertices;
        vertices.reserve((solid.size() * 3) + (solid.size() * 6));

        for (std::size_t i = 0; i < solid.size(); i++)
        {
            const auto next = (i + 1) % solid.size();
            emit_triangle(vertices, center, solid[i], solid[next], fill, fill, fill);
        }

        const auto faded = color { fill.r, fill.g, fill.b, 0 };
        emit_strip(vertices, solid, outer, fill, faded);
        return render_geometry(renderer, vertices);
    }


    [[nodiscard]]
    auto border_raw(SDL_Renderer *renderer, const rect &bounds, color border, float width)
        -> kei::result<>
    {
        const auto requested = std::isfinite(width) ? std::max(0.F, width) : 0.F;
        const auto thickness = std::min({ requested, bounds.size.x / 2.F, bounds.size.y / 2.F });
        if (thickness <= 0.F) return {};

        if (!SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a))
            return sleet::sdl_error { "Failed to set the border color." }.unexpected();

        const SDL_FRect edges[] {
            { bounds.position.x, bounds.position.y, bounds.size.x, thickness },
            { bounds.position.x, bounds.position.y + bounds.size.y - thickness, bounds.size.x,
             thickness },
            { bounds.position.x, bounds.position.y + thickness, thickness,
             std::max(0.F, bounds.size.y - (thickness * 2.F)) },
            { bounds.position.x + bounds.size.x - thickness, bounds.position.y + thickness,
             thickness, std::max(0.F, bounds.size.y - (thickness * 2.F)) },
        };

        for (const auto &edge : edges)
            if (!SDL_RenderFillRect(renderer, &edge))
                return sleet::sdl_error { "Failed to draw a border edge." }.unexpected();
        return {};
    }


    [[nodiscard]]
    auto border_rounded(
        SDL_Renderer *renderer, const rect &bounds, float radius, color border, float width)
        -> kei::result<>
    {
        const auto limit = std::min(bounds.size.x, bounds.size.y) / 2.F;
        radius           = std::clamp(radius, 0.F, std::max(0.F, limit));
        if (radius <= 0.5F) return border_raw(renderer, bounds, border, width);

        const auto requested = std::isfinite(width) ? std::max(0.F, width) : 0.F;
        const auto thickness = std::min({ requested, bounds.size.x / 2.F, bounds.size.y / 2.F });
        if (thickness <= 0.F) return {};

        const auto inner_bounds = bounds.inset(sleet::math::edge_insets { thickness });
        const auto inner_radius = std::max(0.F, radius - thickness);

        const auto outer_fade  = rounded_contour(bounds, radius, 0.5F);
        const auto outer_solid = rounded_contour(bounds, radius, -0.5F);
        const auto inner_solid = rounded_contour(inner_bounds, inner_radius, 0.5F);
        const auto inner_fade  = rounded_contour(inner_bounds, inner_radius, -0.5F);

        const auto clear = color { border.r, border.g, border.b, 0 };

        std::vector<SDL_Vertex> vertices;
        vertices.reserve(outer_fade.size() * 18);

        emit_strip(vertices, outer_solid, outer_fade, border, clear);
        emit_strip(vertices, outer_solid, inner_solid, border, border);
        emit_strip(vertices, inner_solid, inner_fade, border, clear);
        return render_geometry(renderer, vertices);
    }


    [[nodiscard]]
    auto draw_line_aa(SDL_Renderer *renderer, vec2 from, vec2 to, float thickness, color stroke)
        -> kei::result<>
    {
        const auto dx     = to.x - from.x;
        const auto dy     = to.y - from.y;
        const auto length = std::sqrt((dx * dx) + (dy * dy));
        if (length <= 0.0001F) return {};

        const auto nx    = -dy / length;
        const auto ny    = dx / length;
        const auto half  = std::max(0.5F, thickness / 2.F);
        const auto inner = std::max(0.F, half - 0.5F);
        const auto outer = half + 0.5F;

        const auto at = [&](vec2 point, float offset)
        { return SDL_FPoint { point.x + (nx * offset), point.y + (ny * offset) }; };

        const std::vector<SDL_FPoint> inner_contour { at(from, inner), at(to, inner),
                                                      at(to, -inner), at(from, -inner) };
        const std::vector<SDL_FPoint> outer_contour { at(from, outer), at(to, outer),
                                                      at(to, -outer), at(from, -outer) };

        const auto              clear = color { stroke.r, stroke.g, stroke.b, 0 };
        std::vector<SDL_Vertex> vertices;
        vertices.reserve(30);

        // Fill the core of the line first, then feather both edges. Drawing
        // only the inner/outer strips would leave the center transparent.
        emit_triangle(vertices, inner_contour[0], inner_contour[1], inner_contour[2], stroke,
                      stroke, stroke);
        emit_triangle(vertices, inner_contour[0], inner_contour[2], inner_contour[3], stroke,
                      stroke, stroke);
        emit_strip(vertices, inner_contour, outer_contour, stroke, clear);
        return render_geometry(renderer, vertices);
    }
}


struct sleet::render_context::impl
{
    SDL_Renderer *renderer = nullptr;
    widget::theme theme;
    vec2          viewport { 0.F, 0.F };

    /// Physical pixels per logical pixel.
    float pixel_scale = 1.F;

    std::optional<widget::widget_id> hovered;
    std::optional<widget::widget_id> pressed;
    std::optional<widget::widget_id> focused;
    std::optional<widget::widget_id> editing;
    widget::text_selection           selection;

    std::optional<widget::widget_id> preedit_id;
    std::string                      preedit_text;
    std::size_t                      preedit_caret = 0;

    std::size_t cache_clock = 0;

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
    auto to_physical(const rect &logical) const noexcept -> rect
    {
        return {
            { logical.position.x * pixel_scale, logical.position.y * pixel_scale },
            { logical.size.x * pixel_scale,     logical.size.y * pixel_scale     },
        };
    }


    [[nodiscard]]
    auto to_physical(vec2 logical) const noexcept -> vec2
    { return { logical.x * pixel_scale, logical.y * pixel_scale }; }


    void set_pixel_scale(float scale)
    {
        if (!std::isfinite(scale) or scale <= 0.F) scale = 1.F;
        if (std::abs(scale - pixel_scale) < 0.0001F) return;

        pixel_scale = scale;
        clear_text_cache();
        clear_fonts();
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

        const auto     physical = to_physical(rect { 0.F, 0.F, viewport.x, viewport.y });
        const SDL_Rect clip {
            .x = 0,
            .y = 0,
            .w = std::max(0, static_cast<int>(std::ceil(physical.size.x))),
            .h = std::max(0, static_cast<int>(std::ceil(physical.size.y))),
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
    auto is_disabled(const node_data &node) const noexcept -> bool
    {
        for (const auto *current = &node; current != nullptr; current = current->parent)
            if (current->disabled) return true;
        return false;
    }


    [[nodiscard]]
    auto radius_for(const node_data &node) const noexcept -> float
    {
        if (node.visual.corner_radius) return *node.visual.corner_radius;
        if (node.parent == nullptr) return 0.F;

        if (node.control == widget::control_kind::checkbox_indicator) return theme.small_radius;

        if (node.control != widget::control_kind::none or !node.action.empty() or node.focusable)
            return theme.radius;

        // Tinted primitives (slider segments, separators, dots) are usually
        // pieces of a larger shape, so they stay square unless the caller asks
        // for a radius explicitly.
        if (node.tint != widget::tone::none) return 0.F;
        if (node.visual.background or node.visual.border) return theme.radius;
        return 0.F;
    }


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
    auto face_at(const font_spec &spec, int index) const noexcept -> const widget::font_face *
    {
        if (index <= 0) return &theme.fonts.face(spec.role);
        const auto fallback = static_cast<std::size_t>(index - 1);
        if (fallback >= theme.fonts.fallbacks.size()) return nullptr;
        return &theme.fonts.fallbacks[fallback];
    }


    [[nodiscard]]
    auto get_font(const font_spec &spec, int face_index = 0) -> kei::result<TTF_Font *>
    {
        const auto *face = face_at(spec, face_index);
        if (face == nullptr or face->path.empty())
            return sleet::error { "No font configured for the {} role.",
                                  static_cast<int>(spec.role) }
                .unexpected();

        const auto &role_face = theme.fonts.face(spec.role);
        const auto  requested = spec.size.value_or(role_face.size);
        const auto  size
            = std::clamp(std::isfinite(requested) ? requested : 16.F, 1.F, 1024.F) * pixel_scale;
        TTF_FontStyleFlags flags = TTF_STYLE_NORMAL;

        if (spec.weight and *spec.weight >= widget::font_weight::semibold) flags |= TTF_STYLE_BOLD;
        if (spec.attribute.has(widget::text_attribute::italic)) flags |= TTF_STYLE_ITALIC;
        if (spec.attribute.has(widget::text_attribute::underline)) flags |= TTF_STYLE_UNDERLINE;
        if (spec.attribute.has(widget::text_attribute::strikethrough))
            flags |= TTF_STYLE_STRIKETHROUGH;

        const font_key key {
            .role  = spec.role,
            .size  = static_cast<int>(std::lround(size * 100.F)),
            .face  = face_index,
            .flags = flags,
        };

        if (const auto it = fonts.find(key); it != fonts.end()) return it->second;

        auto *font = TTF_OpenFont(face->path.c_str(), size);
        if (font == nullptr)
            return sleet::sdl_error { "Failed to open font '{}' at {} px", face->path, size }
                .unexpected();

        TTF_SetFontStyle(font, flags);
        fonts.emplace(key, font);
        return font;
    }


    [[nodiscard]]
    static auto covers_text(TTF_Font *font, std::string_view text) -> bool
    {
        auto       current = text.begin();
        const auto end     = text.end();

        while (current != end)
        {
            const auto code_point = utf8::unchecked::next(current);
            if (code_point <= 0x20U) continue;
            if (!TTF_FontHasGlyph(font, code_point)) return false;
        }

        return true;
    }


    /// Resolves the primary face, or the first fallback face that covers every
    /// glyph in the run. Mixed-script runs still use one face, but the common
    /// case (a whole line in another script) renders without tofu.
    [[nodiscard]]
    auto get_font_covering(const font_spec &spec, std::string_view text) -> kei::result<TTF_Font *>
    {
        auto primary = get_font(spec, 0);
        if (!primary) return primary;
        if (text.empty() or covers_text(*primary, text)) return primary;

        for (std::size_t i = 0; i < theme.fonts.fallbacks.size(); i++)
        {
            auto fallback = get_font(spec, static_cast<int>(i + 1));
            if (!fallback) continue;
            if (covers_text(*fallback, text)) return fallback;
        }

        return primary;
    }


    [[nodiscard]]
    auto measure_line(TTF_Font *font, std::string_view line) -> kei::result<float>
    {
        if (line.empty()) return 0.F;

        int width  = 0;
        int height = 0;
        if (!TTF_GetStringSize(font, line.data(), line.size(), &width, &height))
            return sleet::sdl_error { "Failed to measure text." }.unexpected();

        return static_cast<float>(width) / pixel_scale;
    }


    [[nodiscard]]
    auto
    wrapped_lines(const font_spec &spec, std::string_view text, std::optional<float> width_limit)
        -> kei::result<std::vector<sleet::txt::wrapped_line>>
    {
        const auto measure = [this, &spec](std::string_view line) -> kei::result<float>
        {
            auto font = get_font_covering(spec, line);
            if (!font) return std::move(font.error()).unexpected();
            return measure_line(*font, line);
        };

        return sleet::txt::wrap(text, width_limit, measure);
    }


    [[nodiscard]]
    auto get_text_texture(std::string_view text, TTF_Font *font, color fg)
        -> kei::result<text_entry *>
    {
        text_key key { .text = std::string { text }, .font = font, .fg = fg };

        if (const auto it = text_cache.find(key); it != text_cache.end())
        {
            it->second.last_used = ++cache_clock;
            return &it->second;
        }

        // Evict the least-recently-used quarter instead of throwing away the
        // whole cache. Re-rendering every visible line after one overflow was
        // the main cause of scroll and resize hitches.
        constexpr auto cache_limit = std::size_t { 2048 };
        if (text_cache.size() >= cache_limit)
        {
            using iterator = decltype(text_cache)::iterator;
            std::vector<iterator> entries;
            entries.reserve(text_cache.size());
            for (auto it = text_cache.begin(); it != text_cache.end(); ++it) entries.push_back(it);

            const auto evict = entries.size() / 4;
            std::nth_element(entries.begin(), entries.begin() + static_cast<std::ptrdiff_t>(evict),
                             entries.end(), [](iterator first, iterator second)
                             { return first->second.last_used < second->second.last_used; });

            for (std::size_t i = 0; i < evict; i++)
            {
                SDL_DestroyTexture(entries[i]->second.texture);
                text_cache.erase(entries[i]);
            }
        }

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

        auto [it, inserted] = text_cache.emplace(std::move(key), text_entry {
                                                                     .texture   = texture,
                                                                     .size      = { width, height },
                                                                     .last_used = ++cache_clock,
        });
        return &it->second;
    }


    [[nodiscard]]
    auto fill_logical(const rect &logical, color fill) -> kei::result<>
    { return fill_raw(renderer, to_physical(logical), fill); }


    [[nodiscard]]
    auto draw_border(const rect &physical, float radius, color border, float width) -> kei::result<>
    { return border_rounded(renderer, physical, radius, border, width); }


    [[nodiscard]]
    auto inherited_node_foreground(const node_data &node) const noexcept -> std::optional<color>
    {
        if (node.control == widget::control_kind::text_input
            or node.control == widget::control_kind::combo_box
            or node.control == widget::control_kind::combo_option)
            if (theme.field.fg) return theme.field.fg;

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
        if (is_disabled(node))
            return theme.disabled.fg.value_or(
                theme.muted.value_or(active.fg.value_or(color { 255, 255, 255, 255 })));
        if (const auto inherited = inherited_foreground(node)) return *inherited;

        if (node.control == widget::control_kind::text_input
            or node.control == widget::control_kind::combo_box
            or node.control == widget::control_kind::combo_option)
            if (theme.field.fg) return *theme.field.fg;

        return active.fg.value_or(color { 255, 255, 255, 255 });
    }


    [[nodiscard]]
    auto variant_background(const node_data &node) const noexcept -> std::optional<color>
    {
        std::optional<color> result;

        switch (node.variant)
        {
        case widget::button_variant::primary:  result = theme.accent; break;
        case widget::button_variant::danger:   result = theme.danger; break;
        case widget::button_variant::ghost:
        case widget::button_variant::standard: break;
        }

        if (!result) return std::nullopt;

        // Accent and danger surfaces need their own hover/pressed feedback;
        // the generic base colors would turn them gray.
        if (is_pressed(node)) return mix(*result, color { 0, 0, 0, 255 }, 0.18F);
        if (is_hovered(node)) return mix(*result, color { 255, 255, 255, 255 }, 0.12F);
        return result;
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
        case widget::control_kind::combo_popup: return theme.base.bg;
        case widget::control_kind::combo_option:
            if (is_pressed(node)) return theme.clicked.bg;
            if (is_hovered(node)) return theme.hover.bg;
            return theme.field.bg ? theme.field.bg : theme.base.bg;
        case widget::control_kind::checkbox_indicator:
        case widget::control_kind::radio_indicator:
            return node.checked ? theme.accent : theme.field.bg;
        case widget::control_kind::slider:
        case widget::control_kind::none:   break;
        }

        return std::nullopt;
    }


    /// The background a node would paint while enabled. Disabled nodes use this
    /// to tell surfaces apart from labels and transparent layout containers.
    [[nodiscard]]
    auto enabled_background(const node_data &node, const colors &active) const noexcept
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

        // Menu rows only paint a background while highlighted.
        if (node.control == widget::control_kind::combo_option) return control_background(node);

        if (const auto variant = variant_background(node)) return variant;
        if (const auto control = control_background(node)) return control;
        if (const auto tinted = theme_color(node.tint)) return tinted;

        if (!node.action.empty() or node.focusable) return active.bg;
        return std::nullopt;
    }


    [[nodiscard]]
    auto background_for(const node_data &node, const colors &active) const noexcept
        -> std::optional<color>
    {
        const auto normal = enabled_background(node, active);
        if (!is_disabled(node)) return normal;

        // Recolor existing surfaces instead of adding one to every descendant;
        // otherwise a disabled container paints a box behind each label. Mute
        // toward the disabled color rather than replacing every surface with
        // it, so thumbs, tracks, and filled indicators keep their shapes.
        if (!normal) return std::nullopt;
        if (!theme.disabled.bg) return normal;
        return mix(*normal, *theme.disabled.bg, 0.6F);
    }


    /// The border a node would paint while enabled.
    [[nodiscard]]
    auto enabled_border(const node_data &node) const noexcept -> std::optional<color>
    {
        if (node.visual.border) return node.visual.border;

        // Sliders draw their own focus ring around the thumb.
        if (node.control == widget::control_kind::slider) return std::nullopt;
        if (focused and *focused == node.id) return theme.focus;

        switch (node.control)
        {
        case widget::control_kind::text_input:
        case widget::control_kind::combo_box:
        case widget::control_kind::combo_popup:        return theme.border;
        case widget::control_kind::checkbox_indicator:
        case widget::control_kind::radio_indicator:
            return node.checked ? theme.accent : theme.border;
        case widget::control_kind::combo_option: return std::nullopt;
        case widget::control_kind::slider:
        case widget::control_kind::none:         break;
        }

        if (node.variant == widget::button_variant::ghost) return std::nullopt;
        if (!node.action.empty()) return theme.border;
        return std::nullopt;
    }


    [[nodiscard]]
    auto border_for(const node_data &node) const noexcept -> std::optional<color>
    {
        const auto normal = enabled_border(node);
        if (!is_disabled(node) or !normal) return normal;
        return theme.disabled_border.value_or(*normal);
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
        const auto physical = to_physical(bounds);
        const auto radius   = radius_for(node) * pixel_scale;

        if (const auto background = background_for(node, active))
            if (auto res = fill_rounded(renderer, physical, radius, *background); !res) return res;

        if (const auto border = border_for(node))
            if (auto res
                = draw_border(physical, radius, *border, border_width_for(node) * pixel_scale);
                !res)
                return res;

        return {};
    }


    [[nodiscard]]
    auto enter_text_clip(const rect &content) -> kei::result<std::optional<text_clip>>
    {
        text_clip state;
        state.had_clip = SDL_RenderClipEnabled(renderer);

        if (state.had_clip and !SDL_GetRenderClipRect(renderer, &state.previous))
            return sleet::sdl_error { "Failed to query the current clip rectangle." }.unexpected();

        auto clip = to_sdl_rect(to_physical(content));
        if (state.had_clip) clip = intersect(clip, state.previous);

        // SDL treats an empty clip rectangle as "clipping disabled". If the
        // content falls completely outside the current clip, or the clip box
        // itself is empty, skip it rather than let it draw over the frame.
        if (clip.w <= 0 or clip.h <= 0) return std::optional<text_clip> {};

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
                        const font_spec         &spec,
                        color                    fg,
                        const txt::wrapped_line &line,
                        std::size_t              index,
                        float                    line_height,
                        float                    y_offset) -> kei::result<>
    {
        if (line.text.empty()) return {};

        auto font = get_font_covering(spec, line.text);
        if (!font) return std::move(font.error()).unexpected();

        auto entry = get_text_texture(line.text, *font, fg);
        if (!entry) return std::move(entry.error()).unexpected();

        const auto logical_height = (*entry)->size.y / pixel_scale;
        const auto x_offset       = align_offset(node.alignment.x, content.size.x - line.width);
        const auto x              = (content.position.x + x_offset) * pixel_scale;
        const auto y = (content.position.y + y_offset + (line_height * static_cast<float>(index))
                        + ((line_height - logical_height) / 2.F))
                     * pixel_scale;

        // Snapping the glyph texture to a physical pixel keeps the rasterized
        // glyphs crisp instead of resampling them at a fractional offset.
        const SDL_FRect destination {
            std::round(x),
            std::round(y),
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
                         const font_spec                      &spec,
                         TTF_Font                             *font,
                         color                                 fg,
                         const std::vector<txt::wrapped_line> &lines) -> kei::result<>
    {
        const auto line_height
            = static_cast<float>(std::max(1, TTF_GetFontHeight(font))) / pixel_scale;
        const auto total_height = line_height * static_cast<float>(lines.size());
        const auto y_offset     = align_offset(node.alignment.y, content.size.y - total_height);

        for (std::size_t i = 0; i < lines.size(); i++)
            if (auto res
                = draw_text_line(node, content, spec, fg, lines[i], i, line_height, y_offset);
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
        return fill_logical(highlight, theme.selection.value_or(color { 47, 95, 168, 255 }));
    }


    [[nodiscard]]
    auto draw_text_input(const node_data &node, const rect &bounds, color fg) -> kei::result<>
    {
        const auto content = bounds.inset(node.padding);
        if (content.size.x <= 0.F or content.size.y <= 0.F) return {};

        const bool is_editing = editing and *editing == node.id;
        const bool has_preedit
            = is_editing and preedit_id and *preedit_id == node.id and !preedit_text.empty();
        const auto text    = std::string_view { node.text };
        const auto display = text.empty() ? (has_preedit ? std::string_view {}
                                                         : std::string_view { node.placeholder })
                                          : text;
        const auto text_color
            = text.empty() ? (is_disabled(node) ? fg : theme.muted.value_or(fg)) : fg;

        auto font = get_font_covering(node.font, display);
        if (!font) return std::move(font.error()).unexpected();

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
            auto computed = text_input_caret(*font, text, content.size.x);
            if (!computed) return restore_after(std::move(computed.error()).unexpected());
            view = *computed;
        }

        if (is_editing)
            if (auto res = draw_text_selection(content, *font, text, view.scroll); !res)
                return restore_after(std::move(res));

        if (!display.empty())
        {
            auto entry = get_text_texture(display, *font, text_color);
            if (!entry) return restore_after(std::move(entry.error()).unexpected());

            const auto      logical_height = (*entry)->size.y / pixel_scale;
            const SDL_FRect destination {
                std::round((content.position.x - view.scroll) * pixel_scale),
                std::round((content.position.y + ((content.size.y - logical_height) / 2.F))
                           * pixel_scale),
                (*entry)->size.x,
                (*entry)->size.y,
            };

            if (!SDL_RenderTexture(renderer, (*entry)->texture, nullptr, &destination))
                return restore_after(sleet::sdl_error { "Failed to draw text." }.unexpected());
        }

        auto caret_x = view.caret_x;

        if (has_preedit)
        {
            auto preedit_font = get_font_covering(node.font, preedit_text);
            if (!preedit_font) return restore_after(std::move(preedit_font.error()).unexpected());

            auto preedit = get_text_texture(preedit_text, *preedit_font, fg);
            if (!preedit) return restore_after(std::move(preedit.error()).unexpected());

            const auto caret_offset = widget::clamp_offset(preedit_text, preedit_caret);
            auto       prefix_width = measure_line(
                *preedit_font, std::string_view { preedit_text }.substr(0, caret_offset));
            if (!prefix_width) return restore_after(std::move(prefix_width.error()).unexpected());

            auto width = measure_line(*preedit_font, preedit_text);
            if (!width) return restore_after(std::move(width.error()).unexpected());

            const auto      logical_height = (*preedit)->size.y / pixel_scale;
            const SDL_FRect destination {
                std::round((content.position.x + caret_x - view.scroll) * pixel_scale),
                std::round((content.position.y + ((content.size.y - logical_height) / 2.F))
                           * pixel_scale),
                (*preedit)->size.x,
                (*preedit)->size.y,
            };

            if (!SDL_RenderTexture(renderer, (*preedit)->texture, nullptr, &destination))
                return restore_after(sleet::sdl_error { "Failed to draw text." }.unexpected());

            const rect underline {
                content.position.x + caret_x - view.scroll,
                content.position.y + content.size.y - 2.F,
                *width,
                1.F,
            };
            if (auto res = fill_logical(underline, theme.caret.value_or(fg)); !res)
                return restore_after(std::move(res));

            caret_x += *prefix_width;
        }

        if (is_editing)
        {
            const rect caret_rect {
                content.position.x + caret_x - view.scroll,
                content.position.y + 2.F,
                2.F,
                std::max(0.F, content.size.y - 4.F),
            };

            if (auto res = fill_logical(caret_rect, theme.caret.value_or(fg)); !res)
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
            return draw_text_input(node, bounds, fg);

        if (content.size.x <= 0.F or content.size.y <= 0.F) return {};

        auto lines = wrapped_lines(node.font, node.text, content.size.x);
        if (!lines) return std::move(lines.error()).unexpected();
        if (lines->empty()) return {};

        auto clip = enter_text_clip(content);
        if (!clip) return std::move(clip.error()).unexpected();
        if (!*clip) return {};

        auto drawn    = draw_text_lines(node, content, node.font, *font, fg, *lines);
        auto restored = leave_text_clip(**clip);

        if (!drawn) return drawn;
        if (!restored) return restored;
        return {};
    }


    [[nodiscard]]
    auto draw_check(const rect &physical, color stroke) -> kei::result<>
    {
        const auto x = physical.position.x;
        const auto y = physical.position.y;
        const auto w = physical.size.x;
        const auto h = physical.size.y;

        if (auto res
            = draw_line_aa(renderer, { x + (w * 0.20F), y + (h * 0.52F) },
                           { x + (w * 0.42F), y + (h * 0.75F) }, 2.F * pixel_scale, stroke);
            !res)
            return res;

        return draw_line_aa(renderer, { x + (w * 0.42F), y + (h * 0.75F) },
                            { x + (w * 0.82F), y + (h * 0.26F) }, 2.F * pixel_scale, stroke);
    }


    [[nodiscard]]
    auto draw_indicator(const node_data &node, const rect &physical_bounds) -> kei::result<>
    {
        if (!node.checked) return {};

        const auto stroke
            = is_disabled(node)
                ? theme.disabled.fg.value_or(
                      theme.muted.value_or(theme.accent_fg.value_or(color { 255, 255, 255, 255 })))
                : theme.accent_fg.value_or(theme.base.fg.value_or(color { 255, 255, 255, 255 }));

        switch (node.control)
        {
        case widget::control_kind::checkbox_indicator:
            return draw_check(physical_bounds.inset(sleet::math::edge_insets { 3.F * pixel_scale }),
                              stroke);
        case widget::control_kind::radio_indicator:
            {
                const auto inner
                    = physical_bounds.inset(sleet::math::edge_insets { 5.F * pixel_scale });
                return fill_rounded(renderer, inner, std::min(inner.size.x, inner.size.y) / 2.F,
                                    stroke);
            }
        case widget::control_kind::none:
        case widget::control_kind::text_input:
        case widget::control_kind::combo_box:
        case widget::control_kind::combo_popup:
        case widget::control_kind::combo_option:
        case widget::control_kind::slider:       return {};
        }

        return {};
    }


    [[nodiscard]]
    auto draw_combo_chevron(const node_data &node, const rect &bounds) -> kei::result<>
    {
        const auto physical = to_physical(bounds);
        if (physical.size.x <= 0.F or physical.size.y <= 0.F) return {};

        const auto scale       = pixel_scale;
        const auto half_width  = 4.F * scale;
        const auto half_height = 2.5F * scale;
        const auto center_x    = physical.right() - (node.padding.right * scale) - half_width;
        const auto center_y    = physical.center().y;
        const auto stroke      = foreground_for(node, active_colors(node));
        const auto thickness   = 1.5F * scale;

        if (auto res = draw_line_aa(renderer, { center_x - half_width, center_y - half_height },
                                    { center_x, center_y + half_height }, thickness, stroke);
            !res)
            return res;

        return draw_line_aa(renderer, { center_x, center_y + half_height },
                            { center_x + half_width, center_y - half_height }, thickness, stroke);
    }


    [[nodiscard]]
    auto draw_slider(const node_data &node, const rect &bounds) -> kei::result<>
    {
        const auto physical = to_physical(bounds);
        if (physical.size.x <= 0.F or physical.size.y <= 0.F) return {};

        const auto disabled = is_disabled(node);
        const auto mute     = [&](color value)
        {
            if (!disabled or !theme.disabled.bg) return value;
            return mix(value, *theme.disabled.bg, 0.6F);
        };

        auto track_color
            = mute(theme.field.bg.value_or(theme.base.bg.value_or(color { 0, 0, 0, 255 })));
        auto       fill_color  = mute(theme.accent.value_or(track_color));
        const auto thumb_color = mute(theme.accent_fg.value_or(color { 255, 255, 255, 255 }));

        if (!disabled and (is_hovered(node) or is_pressed(node)))
        {
            if (theme.hover.bg) track_color = mix(track_color, *theme.hover.bg, 0.5F);
            fill_color = mix(fill_color, color { 255, 255, 255, 255 }, 0.08F);
        }

        const auto border_color
            = disabled ? theme.disabled_border.value_or(theme.border.value_or(track_color))
                       : theme.border.value_or(track_color);
        const auto thumb_border
            = disabled ? theme.disabled_border.value_or(theme.border.value_or(thumb_color))
                       : theme.border.value_or(theme.accent.value_or(thumb_color));

        const auto scale        = pixel_scale;
        const auto track_height = std::min(widget::slider_track_height * scale, physical.size.y);
        const auto center_y     = physical.center().y;
        const auto track        = rect {
            physical.position.x,
            center_y - (track_height / 2.F),
            physical.size.x,
            track_height,
        };

        if (auto res = fill_rounded(renderer, track, track_height / 2.F, track_color); !res)
            return res;

        const auto thumb_radius = std::min(widget::slider_thumb_radius * scale,
                                           std::min(physical.size.x, physical.size.y) / 2.F);
        const auto travel       = std::max(0.F, physical.size.x - (2.F * thumb_radius));
        const auto thumb_center = physical.position.x + thumb_radius
                                + (std::clamp(node.control_value, 0.F, 1.F) * travel);
        const auto fill_width   = thumb_center - physical.position.x;

        if (fill_width > 0.F)
            if (auto res = fill_rounded(
                    renderer,
                    rect { physical.position.x, track.position.y, fill_width, track_height },
                    track_height / 2.F, fill_color);
                !res)
                return res;

        if (theme.border)
            if (auto res = border_rounded(renderer, track, track_height / 2.F, border_color,
                                          std::max(1.F, scale));
                !res)
                return res;

        const auto thumb = rect {
            thumb_center - thumb_radius,
            center_y - thumb_radius,
            thumb_radius * 2.F,
            thumb_radius * 2.F,
        };

        if (auto res = fill_rounded(renderer, thumb, thumb_radius, thumb_color); !res) return res;
        if (auto res
            = border_rounded(renderer, thumb, thumb_radius, thumb_border, std::max(1.F, scale));
            !res)
            return res;

        if (!disabled and focused and *focused == node.id)
        {
            const auto ring = thumb.inset(sleet::math::edge_insets { -2.F * scale });
            if (auto res = border_rounded(renderer, ring, thumb_radius + (2.F * scale),
                                          theme.focus.value_or(fill_color), 2.F * scale);
                !res)
                return res;
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

        const auto active   = active_colors(node);
        const auto physical = to_physical(*bounds);
        const auto radius   = radius_for(node) * pixel_scale;

        if (const auto background = background_for(node, active))
            if (auto res = fill_rounded(renderer, physical, radius, *background); !res) return res;

        if (const auto border = border_for(node))
            if (auto res
                = draw_border(physical, radius, *border, border_width_for(node) * pixel_scale);
                !res)
                return res;

        if (node.control == widget::control_kind::checkbox_indicator
            or node.control == widget::control_kind::radio_indicator)
            if (auto res = draw_indicator(node, physical); !res) return res;

        if (node.control == widget::control_kind::slider)
            if (auto res = draw_slider(node, *bounds); !res) return res;

        if (node.control == widget::control_kind::combo_box)
            if (auto res = draw_combo_chevron(node, *bounds); !res) return res;

        auto clip = std::optional<text_clip> {};
        if (node.clip_children)
        {
            auto entered = enter_text_clip(*bounds);
            if (!entered) return std::move(entered.error()).unexpected();
            if (!*entered) return {};
            clip = **entered;
        }

        for (const auto &child : node.children)
        {
            if (child.floating) continue;
            if (auto result = draw_node(child, layout); !result)
            {
                if (clip) (void)leave_text_clip(*clip);
                return std::move(result.error()).unexpected();
            }
        }

        if (clip)
            if (auto res = leave_text_clip(*clip); !res) return res;

        return {};
    }


    /// Applies every clipping ancestor of a floating node. Floating nodes are
    /// painted in a separate pass, so they cannot rely on draw_node's recursive
    /// clip stack.
    [[nodiscard]]
    auto enter_ancestor_clips(const node_data &node, const widget::layout_result &layout)
        -> kei::result<std::optional<std::vector<text_clip>>>
    {
        auto states = std::vector<text_clip> {};

        for (auto *current = node.parent; current != nullptr; current = current->parent)
        {
            if (!current->clip_children) continue;

            const auto *bounds = layout.find(current->id);
            if (bounds == nullptr) continue;

            auto entered = enter_text_clip(*bounds);
            if (!entered) return std::move(entered.error()).unexpected();
            if (!*entered)
            {
                for (auto it = states.rbegin(); it != states.rend(); ++it)
                    if (auto res = leave_text_clip(*it); !res)
                        return std::move(res.error()).unexpected();
                return std::optional<std::vector<text_clip>> {};
            }

            states.push_back(**entered);
        }

        return std::optional<std::vector<text_clip>> { std::move(states) };
    }


    [[nodiscard]]
    auto draw_floating(const node_data &node, const widget::layout_result &layout) -> kei::result<>
    {
        // A floating node is still part of its parent's subtree. Hide it when
        // any ancestor is hidden; layout entries alone do not carry that state.
        for (const auto *current = &node; current != nullptr; current = current->parent)
            if (!current->visible) return {};

        auto clips = enter_ancestor_clips(node, layout);
        if (!clips) return std::move(clips.error()).unexpected();
        if (!*clips) return {};

        auto result = draw_node(node, layout);

        for (auto it = (*clips)->rbegin(); it != (*clips)->rend(); ++it)
        {
            auto restored = leave_text_clip(*it);
            if (!result) continue;
            if (!restored) result = std::move(restored.error()).unexpected();
        }

        return result;
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

    auto lines = m_impl->wrapped_lines(font, text, width_limit);
    if (!lines) return std::move(lines.error()).unexpected();
    if (lines->empty()) return vec2 { 0.F, 0.F };

    float width = 0.F;
    for (const auto &line : *lines) width = std::max(width, line.width);

    const auto font_height  = static_cast<float>(std::max(1, TTF_GetFontHeight(*ttf_font)));
    const auto line_height  = font_height / m_impl->pixel_scale;
    const auto total_height = line_height * static_cast<float>(lines->size());
    return vec2 { width, total_height };
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

    auto result = m_impl->draw_node(root, layout);

    // Floating content (combo-box popups and other overlays) is painted above
    // every in-flow node, in ascending z-order.
    if (result)
        for (const auto &entry : layout.floating)
        {
            const auto *node = root.find(entry.id);
            if (node == nullptr or node == &root) continue;
            if (auto drawn = m_impl->draw_floating(*node, layout); !drawn)
            {
                result = std::move(drawn.error()).unexpected();
                break;
            }
        }

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

void sleet::render_context::set_pixel_scale(float scale) noexcept
{ m_impl->set_pixel_scale(scale); }

void sleet::render_context::set_preedit(std::optional<widget::widget_id> id,
                                        std::string                      text,
                                        std::size_t                      caret) noexcept
{
    m_impl->preedit_id    = std::move(id);
    m_impl->preedit_text  = std::move(text);
    m_impl->preedit_caret = caret;
}
