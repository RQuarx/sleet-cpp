#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <boost/ut.hpp>

#include "sleet/render_context.hh"
#include "sleet/widget/layout.hh"
#include "sleet/widget/modifier.hh"
#include "sleet/widget/theme.hh"
#include "sleet/widgets.hh"


const boost::ut::suite render_tests = []
{
    using namespace boost::ut;
    using namespace sleet::widget;
    using namespace sleet::literals;

    "a background is rasterized into the target"_test = []
    {
        if (!SDL_Init(SDL_INIT_VIDEO)) return;
        if (!TTF_Init())
        {
            SDL_Quit();
            return;
        }

        auto cleanup = [](SDL_Window *window, SDL_Renderer *renderer)
        {
            if (renderer != nullptr) SDL_DestroyRenderer(renderer);
            if (window != nullptr) SDL_DestroyWindow(window);
            TTF_Quit();
            SDL_Quit();
        };

        auto *window = SDL_CreateWindow("sleet-render-test", 64, 64, SDL_WINDOW_HIDDEN);
        if (window == nullptr)
        {
            cleanup(nullptr, nullptr);
            return;
        }

        auto *renderer = SDL_CreateRenderer(window, nullptr);
        if (renderer == nullptr)
        {
            cleanup(window, nullptr);
            return;
        }

        auto theme = theme::default_dark();
        if (theme.fonts.face(font_role::body).path.empty())
        {
            cleanup(window, renderer);
            return;
        }

        const auto clear_color = theme.base.bg.value_or(sleet::color { 0, 0, 0, 255 });
        const auto base_bg     = theme.base.bg.value();
        const auto hover_bg    = theme.hover.bg.value();
        const auto toast_bg    = theme.toast_bg.value();

        try
        {
            auto context = sleet::render_context { renderer, std::move(theme) };
            context.set_viewport({ 64.F, 64.F });

            auto tree = to_data(box() | size(sizing::fill()) | background(0xFF0000_rgb));
            expect(static_cast<bool>(tree.finalize()));

            const auto measure
                = [&context](std::string_view text, const font_spec &font,
                             std::optional<float> width_limit, sleet::widget::text_wrap_mode wrap)
            { return context.measure_text(text, font, width_limit, wrap); };

            auto layout = calculate_layout(tree, measure, { 64.F, 64.F });
            expect(static_cast<bool>(layout));
            if (!layout)
            {
                cleanup(window, renderer);
                return;
            }

            expect(static_cast<bool>(context.render(tree, *layout)));

            // Layout exposes byte offsets so the runtime can map carets and
            // pointer positions onto the same lines the renderer draws.
            auto measured_lines
                = context.measure_lines("ab\ncd", font_spec {}, 100.F, text_wrap_mode::wrap);
            expect(static_cast<bool>(measured_lines));
            if (measured_lines)
            {
                expect(eq(measured_lines->size(), 2_u));
                expect(eq((*measured_lines)[0].begin, 0_u));
                expect(eq((*measured_lines)[0].end, 2_u));
                expect(eq((*measured_lines)[1].begin, 3_u));
                expect(eq((*measured_lines)[1].end, 5_u));
            }

            auto empty_lines
                = context.measure_lines({}, font_spec {}, 100.F, text_wrap_mode::wrap);
            expect(static_cast<bool>(empty_lines));
            if (empty_lines)
            {
                expect(eq(empty_lines->size(), 1_u));
                expect(eq((*empty_lines)[0].begin, 0_u));
                expect(eq((*empty_lines)[0].end, 0_u));
            }

            // A trailing newline leaves an empty final line for the caret.
            auto trailing_lines
                = context.measure_lines("ab\n", font_spec {}, 100.F, text_wrap_mode::wrap);
            expect(static_cast<bool>(trailing_lines));
            if (trailing_lines)
            {
                expect(eq(trailing_lines->size(), 2_u));
                expect(eq((*trailing_lines)[1].begin, 3_u));
                expect(eq((*trailing_lines)[1].end, 3_u));
            }

            auto *background_pixels = SDL_RenderReadPixels(renderer, nullptr);
            expect(background_pixels != nullptr);
            if (background_pixels != nullptr)
            {
                std::uint8_t r = 0;
                std::uint8_t g = 0;
                std::uint8_t b = 0;
                std::uint8_t a = 0;
                expect(SDL_ReadSurfacePixel(background_pixels, 32, 32, &r, &g, &b, &a));
                expect(r == 0xFF);
                expect(g == 0x00);
                expect(b == 0x00);
                expect(a == 0xFF);
                SDL_DestroySurface(background_pixels);
            }

            // Text completely outside the viewport must not fall back to an
            // unclipped draw when the text clip rectangle is empty.
            context.set_viewport({ 32.F, 32.F });

            auto clipped = to_data(row()[box() | width(40.F) | height(64.F),
                                         label("MMMM") | text_size(32.F) | text_color(0x00FF00_rgb)]
                                   | size(sizing::fill()) | background(0xFF0000_rgb));
            expect(static_cast<bool>(clipped.finalize()));

            auto clipped_layout = calculate_layout(clipped, measure, { 64.F, 64.F });
            expect(static_cast<bool>(clipped_layout));
            if (clipped_layout)
            {
                expect(static_cast<bool>(context.render(clipped, *clipped_layout)));

                auto *clipped_pixels = SDL_RenderReadPixels(renderer, nullptr);
                expect(clipped_pixels != nullptr);
                if (clipped_pixels != nullptr)
                {
                    bool drew_text = false;

                    for (int y = 0; y < 32 and !drew_text; y++)
                        for (int x = 40; x < 64 and !drew_text; x++)
                        {
                            std::uint8_t r = 0;
                            std::uint8_t g = 0;
                            std::uint8_t b = 0;
                            std::uint8_t a = 0;
                            if (!SDL_ReadSurfacePixel(clipped_pixels, x, y, &r, &g, &b, &a))
                                continue;

                            drew_text = r != clear_color.r or g != clear_color.g
                                     or b != clear_color.b or a != clear_color.a;
                        }

                    expect(!drew_text) << "text was drawn outside the viewport";
                    SDL_DestroySurface(clipped_pixels);
                }
            }

            // Controls exercise focus borders, indicators, placeholder text,
            // selection highlighting, and the slider track.
            context.set_viewport({ 320.F, 240.F });

            auto controls = to_data(
                column()[text_input("hello", "name", "Type here...") | id("field"),
                         checkbox("Check", true, "check"), radio_button("Radio", false, "radio"),
                         combo_box("Green", { "Red", "Green" }, "color"),
                         slider(0.5F, "volume") | id("slider")]
                | padding(8.F) | gap(4.F) | size(sizing::fill()));
            expect(static_cast<bool>(controls.finalize()));

            auto controls_layout = calculate_layout(controls, measure, { 320.F, 240.F });
            expect(static_cast<bool>(controls_layout));
            if (controls_layout)
            {
                const auto field = widget_id { "field" };
                context.set_focused(field);
                context.set_text_selection(field, text_selection { .caret = 5, .anchor = 0 });

                expect(static_cast<bool>(context.render(controls, *controls_layout)));

                context.set_focused(std::nullopt);
                context.set_text_selection(std::nullopt, {});
            }

            // A multiline field draws wrapped lines and places its caret on
            // the line that owns the offset.
            auto area_tree = to_data(column()[text_area("ab\ncd", "notes") | id("notes")
                                              | width(200.F) | height(80.F)]
                                     | size(sizing::fill()));
            expect(static_cast<bool>(area_tree.finalize()));

            auto area_layout = calculate_layout(area_tree, measure, { 320.F, 240.F });
            expect(static_cast<bool>(area_layout));
            if (area_layout)
            {
                const auto area_id = widget_id { "notes" };
                const auto *area   = area_tree.find(area_id);
                const auto *bounds = area_layout->find(area_id);
                expect(area != nullptr);
                expect(bounds != nullptr);

                if (area != nullptr and bounds != nullptr)
                {
                    const text_selection selection { .caret = 3, .anchor = 1 };
                    context.set_focused(area_id);
                    context.set_text_selection(area_id, selection, 0.F);

                    expect(static_cast<bool>(context.render(area_tree, *area_layout)));

                    auto caret = context.text_input_caret_rect(*area, *bounds, selection);
                    expect(caret.has_value());
                    if (caret)
                    {
                        auto line_height = context.line_height(area->font);
                        expect(static_cast<bool>(line_height));
                        if (line_height)
                        {
                            const auto content = bounds->inset(area->padding);
                            expect(std::abs(caret->position.x - content.position.x) < 1.F);
                            expect(std::abs(caret->position.y
                                            - (content.position.y + *line_height + 2.F))
                                   < 1.F);
                        }
                    }

                    context.set_focused(std::nullopt);
                    context.set_text_selection(std::nullopt, {});
                }
            }

            // The caret after a trailing newline sits on the new empty line,
            // not at the end of the previous line.
            auto trailing_tree = to_data(column()[text_area("ab\n", "trailing") | id("trailing")
                                                  | width(200.F) | height(80.F)]
                                         | size(sizing::fill()));
            expect(static_cast<bool>(trailing_tree.finalize()));

            auto trailing_layout = calculate_layout(trailing_tree, measure, { 320.F, 240.F });
            expect(static_cast<bool>(trailing_layout));
            if (trailing_layout)
            {
                const auto trailing_id = widget_id { "trailing" };
                const auto *trailing   = trailing_tree.find(trailing_id);
                const auto *bounds     = trailing_layout->find(trailing_id);
                expect(trailing != nullptr);
                expect(bounds != nullptr);

                if (trailing != nullptr and bounds != nullptr)
                {
                    const text_selection selection { .caret = 3, .anchor = 3 };
                    context.set_focused(trailing_id);
                    context.set_text_selection(trailing_id, selection, 0.F);

                    auto caret = context.text_input_caret_rect(*trailing, *bounds, selection);
                    expect(caret.has_value());
                    if (caret)
                    {
                        auto line_height = context.line_height(trailing->font);
                        expect(static_cast<bool>(line_height));
                        if (line_height)
                        {
                            const auto content = bounds->inset(trailing->padding);
                            expect(std::abs(caret->position.x - content.position.x) < 1.F);
                            expect(std::abs(caret->position.y
                                            - (content.position.y + *line_height + 2.F))
                                   < 1.F);
                        }
                    }

                    context.set_focused(std::nullopt);
                    context.set_text_selection(std::nullopt, {});
                }
            }

            // The slider is one rounded track with an accent fill and a
            // centered thumb, not three adjacent rectangles.
            auto slider_tree
                = to_data(box()[slider(0.5F, "volume") | id("slider")] | size(60.F, 20.F));
            expect(static_cast<bool>(slider_tree.finalize()));

            auto slider_layout = calculate_layout(slider_tree, measure, { 64.F, 64.F });
            expect(static_cast<bool>(slider_layout));
            if (slider_layout)
            {
                expect(static_cast<bool>(context.render(slider_tree, *slider_layout)));

                if (const auto *slider = slider_layout->find("slider"); slider != nullptr)
                {
                    auto *pixels = SDL_RenderReadPixels(renderer, nullptr);
                    expect(pixels != nullptr);
                    if (pixels != nullptr)
                    {
                        const auto sample = [pixels](int x, int y)
                        {
                            std::uint8_t r = 0;
                            std::uint8_t g = 0;
                            std::uint8_t b = 0;
                            std::uint8_t a = 0;
                            expect(SDL_ReadSurfacePixel(pixels, x, y, &r, &g, &b, &a));
                            return sleet::color { r, g, b, a };
                        };

                        const auto center_y
                            = static_cast<int>(slider->position.y + (slider->size.y / 2.F));
                        const auto fill_x
                            = static_cast<int>(slider->position.x + (slider->size.x * 0.25F));
                        const auto thumb_x
                            = static_cast<int>(slider->position.x + (slider->size.x * 0.5F));

                        const auto fill  = sample(fill_x, center_y);
                        const auto thumb = sample(thumb_x, center_y);

                        expect(fill == 0x3B82F6_rgb);
                        expect(thumb == 0xFFFFFF_rgb);
                        SDL_DestroySurface(pixels);
                    }
                }
            }

            // Eased interaction progress blends hover feedback instead of
            // switching colors instantly.
            context.set_viewport({ 64.F, 64.F });

            auto animated_button = to_data(button("Go", "go") | id("animated") | width(80.F));
            expect(static_cast<bool>(animated_button.finalize()));

            auto animated_layout = calculate_layout(animated_button, measure, { 64.F, 64.F });
            expect(static_cast<bool>(animated_layout));
            if (animated_layout)
            {
                context.set_interaction_progress({
                    { widget_id { "animated" }, sleet::interaction_progress { .hover = 0.5F } }
                });

                expect(static_cast<bool>(context.render(animated_button, *animated_layout)));

                auto *pixels = SDL_RenderReadPixels(renderer, nullptr);
                expect(pixels != nullptr);
                if (pixels != nullptr)
                {
                    std::uint8_t r = 0;
                    std::uint8_t g = 0;
                    std::uint8_t b = 0;
                    std::uint8_t a = 0;
                    expect(SDL_ReadSurfacePixel(pixels, 10, 10, &r, &g, &b, &a));

                    const auto expected = base_bg.lerp(hover_bg, 0.5F);
                    expect(r == expected.r);
                    expect(g == expected.g);
                    expect(b == expected.b);
                    SDL_DestroySurface(pixels);
                }

                context.set_interaction_progress({});
            }

            // Scrollable viewports paint a proportional scrollbar above their
            // content.
            context.set_viewport({ 64.F, 64.F });

            auto scroll_tree
                = to_data(column()[box() | width(64.F) | height(200.F) | background(0x303030_rgb)]
                          | id("scroll") | size(64.F, 64.F) | clip() | scrollable());
            expect(static_cast<bool>(scroll_tree.finalize()));

            auto scroll_layout = calculate_layout(scroll_tree, measure, { 64.F, 64.F });
            expect(static_cast<bool>(scroll_layout));
            if (scroll_layout)
            {
                context.set_scroll_metrics({
                    { widget_id { "scroll" }, scroll_metrics { .offset   = { 0.F, 50.F },
                                                               .max      = { 0.F, 100.F },
                                                               .viewport = { 64.F, 64.F } } }
                });

                expect(static_cast<bool>(context.render(scroll_tree, *scroll_layout)));

                auto *pixels = SDL_RenderReadPixels(renderer, nullptr);
                expect(pixels != nullptr);
                if (pixels != nullptr)
                {
                    const auto sample = [pixels](int x, int y)
                    {
                        std::uint8_t r = 0;
                        std::uint8_t g = 0;
                        std::uint8_t b = 0;
                        std::uint8_t a = 0;
                        expect(SDL_ReadSurfacePixel(pixels, x, y, &r, &g, &b, &a));
                        return sleet::color { r, g, b, a };
                    };

                    const auto track = sample(59, 5);
                    const auto thumb = sample(59, 30);

                    expect(track == 0x2A2E35_rgb);
                    expect(thumb == 0x5A626D_rgb);
                    SDL_DestroySurface(pixels);
                }

                context.set_scroll_metrics({});
            }

            // Tooltips draw a bubble under their anchor, clamped to the
            // viewport, and clear on the next frame.
            context.set_viewport({ 64.F, 64.F });

            auto plain = to_data(box() | size(sizing::fill()) | background(0xFF0000_rgb));
            expect(static_cast<bool>(plain.finalize()));

            auto plain_layout = calculate_layout(plain, measure, { 64.F, 64.F });
            expect(static_cast<bool>(plain_layout));
            if (plain_layout)
            {
                auto spec = font_spec {};
                spec.role = font_role::caption;

                const auto tip_size = context.measure_text("Tip", spec, std::nullopt);
                expect(static_cast<bool>(tip_size));
                if (tip_size)
                {
                    const auto bubble_w = tip_size->x + 16.F;
                    const auto bubble_h = tip_size->y + 10.F;
                    const auto anchor   = sleet::math::rect { 20.F, 10.F, 24.F, 10.F };
                    const auto bubble_x = anchor.center().x - (bubble_w / 2.F);
                    const auto bubble_y = anchor.bottom() + 6.F;

                    context.set_tooltip("Tip", anchor);
                    expect(static_cast<bool>(context.render(plain, *plain_layout)));

                    auto *pixels = SDL_RenderReadPixels(renderer, nullptr);
                    expect(pixels != nullptr);
                    if (pixels != nullptr)
                    {
                        const auto sample = [pixels](int x, int y)
                        {
                            std::uint8_t r = 0;
                            std::uint8_t g = 0;
                            std::uint8_t b = 0;
                            std::uint8_t a = 0;
                            expect(SDL_ReadSurfacePixel(pixels, x, y, &r, &g, &b, &a));
                            return sleet::color { r, g, b, a };
                        };

                        expect(sample(static_cast<int>(bubble_x) + 2,
                                      static_cast<int>(bubble_y + (bubble_h / 2.F)))
                               == toast_bg);
                        expect(sample(32, 4) == 0xFF0000_rgb);
                        SDL_DestroySurface(pixels);
                    }

                    context.clear_tooltip();
                }
            }

            // Toasts stack above the content and carry an accent bar.
            context.set_viewport({ 64.F, 64.F });
            context.set_toasts({
                sleet::toast_message { "Saved", 1.F, 0.F }
            });

            if (plain_layout)
            {
                expect(static_cast<bool>(context.render(plain, *plain_layout)));

                auto *pixels = SDL_RenderReadPixels(renderer, nullptr);
                expect(pixels != nullptr);
                if (pixels != nullptr)
                {
                    const auto sample = [pixels](int x, int y)
                    {
                        std::uint8_t r = 0;
                        std::uint8_t g = 0;
                        std::uint8_t b = 0;
                        std::uint8_t a = 0;
                        expect(SDL_ReadSurfacePixel(pixels, x, y, &r, &g, &b, &a));
                        return sleet::color { r, g, b, a };
                    };

                    expect(sample(60, 20) == 0xFF0000_rgb);
                    expect(sample(40, 20) == toast_bg);
                    SDL_DestroySurface(pixels);
                }

                context.set_toasts({});
            }

            // Floating content is painted above normal flow, even when it
            // extends past its parent's bounds.
            context.set_viewport({ 64.F, 64.F });

            auto overlay = to_data(column()[box() | size(64.F, 20.F) | background(0x00FF00_rgb),
                                            box() | size(64.F, 20.F) | background(0xFF0000_rgb)
                                                | floating() | placement(0.F, 30.F)]
                                   | size(64.F, 64.F));
            expect(static_cast<bool>(overlay.finalize()));

            auto overlay_layout = calculate_layout(overlay, measure, { 64.F, 64.F });
            expect(static_cast<bool>(overlay_layout));
            if (overlay_layout)
            {
                expect(static_cast<bool>(context.render(overlay, *overlay_layout)));

                auto *pixels = SDL_RenderReadPixels(renderer, nullptr);
                expect(pixels != nullptr);
                if (pixels != nullptr)
                {
                    std::uint8_t r = 0;
                    std::uint8_t g = 0;
                    std::uint8_t b = 0;
                    std::uint8_t a = 0;
                    expect(SDL_ReadSurfacePixel(pixels, 32, 35, &r, &g, &b, &a));
                    expect(r == 0xFF);
                    expect(g == 0x00);
                    expect(b == 0x00);
                    SDL_DestroySurface(pixels);
                }
            }

            // An empty clip box must disable drawing rather than fall back to
            // SDL's "empty clip means no clipping" behavior.
            auto empty_clip = to_data(column()[box() | size(20.F) | background(0xFF0000_rgb)]
                                      | size(0.F) | clip());
            expect(static_cast<bool>(empty_clip.finalize()));

            auto empty_clip_layout = calculate_layout(empty_clip, measure, { 64.F, 64.F });
            expect(static_cast<bool>(empty_clip_layout));
            if (empty_clip_layout)
            {
                expect(static_cast<bool>(context.render(empty_clip, *empty_clip_layout)));

                auto *pixels = SDL_RenderReadPixels(renderer, nullptr);
                expect(pixels != nullptr);
                if (pixels != nullptr)
                {
                    std::uint8_t r = 0;
                    std::uint8_t g = 0;
                    std::uint8_t b = 0;
                    std::uint8_t a = 0;
                    expect(SDL_ReadSurfacePixel(pixels, 5, 5, &r, &g, &b, &a));
                    expect(!(r == 0xFF and g == 0x00 and b == 0x00))
                        << "a child escaped an empty clip";
                    SDL_DestroySurface(pixels);
                }
            }

            // Text alignment moves glyphs without moving the widget box.
            context.set_viewport({ 64.F, 32.F });

            auto aligned
                = to_data(box()[label("I") | size(sizing::fill()) | text_size(24.F)
                                | text_color(0xFFFFFF_rgb) | text_align(text_alignment::right)]
                          | size(64.F, 32.F) | background(0x000000_rgb));
            expect(static_cast<bool>(aligned.finalize()));

            auto aligned_layout = calculate_layout(aligned, measure, { 64.F, 32.F });
            expect(static_cast<bool>(aligned_layout));
            if (aligned_layout)
            {
                expect(static_cast<bool>(context.render(aligned, *aligned_layout)));

                auto *pixels = SDL_RenderReadPixels(renderer, nullptr);
                expect(pixels != nullptr);
                if (pixels != nullptr)
                {
                    auto min_x = 64;
                    for (int y = 0; y < 32; y++)
                        for (int x = 0; x < 64; x++)
                        {
                            std::uint8_t r = 0;
                            std::uint8_t g = 0;
                            std::uint8_t b = 0;
                            std::uint8_t a = 0;
                            if (!SDL_ReadSurfacePixel(pixels, x, y, &r, &g, &b, &a)) continue;
                            if (r + g + b > 30) min_x = std::min(min_x, x);
                        }

                    expect(min_x > 32) << "right-aligned text did not reach the right half";
                    SDL_DestroySurface(pixels);
                }
            }

            // Justification expands spaces on soft-wrapped lines but leaves
            // the final line of a paragraph at its natural width.
            context.set_viewport({ 64.F, 40.F });

            const auto justified_tree = [](text_alignment alignment)
            {
                return to_data(box()[label("aa aa aaaaaa") | size(sizing::fill()) | text_size(12.F)
                                     | text_color(0xFFFFFF_rgb) | text_align(alignment)]
                               | size(64.F, 40.F) | background(0x000000_rgb));
            };

            const auto first_line_extent = [&](const sleet::widget::node_data     &text_tree,
                                               const sleet::widget::layout_result &text_layout)
            {
                expect(static_cast<bool>(context.render(text_tree, text_layout)));

                auto  max_x  = 0;
                auto *pixels = SDL_RenderReadPixels(renderer, nullptr);
                expect(pixels != nullptr);
                if (pixels != nullptr)
                {
                    for (int y = 0; y < 12; y++)
                        for (int x = 0; x < 64; x++)
                        {
                            std::uint8_t r = 0;
                            std::uint8_t g = 0;
                            std::uint8_t b = 0;
                            std::uint8_t a = 0;
                            if (!SDL_ReadSurfacePixel(pixels, x, y, &r, &g, &b, &a)) continue;
                            if (r + g + b > 30) max_x = std::max(max_x, x);
                        }

                    SDL_DestroySurface(pixels);
                }
                return max_x;
            };

            auto left_tree = justified_tree(text_alignment::left);
            expect(static_cast<bool>(left_tree.finalize()));
            auto left_layout = calculate_layout(left_tree, measure, { 64.F, 40.F });
            expect(static_cast<bool>(left_layout));

            auto justify_tree = justified_tree(text_alignment::justify);
            expect(static_cast<bool>(justify_tree.finalize()));
            auto justify_layout = calculate_layout(justify_tree, measure, { 64.F, 40.F });
            expect(static_cast<bool>(justify_layout));

            if (left_layout and justify_layout)
            {
                const auto left_max    = first_line_extent(left_tree, *left_layout);
                const auto justify_max = first_line_extent(justify_tree, *justify_layout);

                expect(justify_max > left_max + 8) << "justification did not expand the first line";
            }

            // The IME caret rectangle follows the field's text alignment.
            context.set_viewport({ 120.F, 40.F });

            auto caret_tree
                = to_data(box()[text_input("abcd", "field", "Name") | id("field") | width(100.F)
                                | height(24.F) | text_align(text_alignment::right)]
                          | size(120.F, 40.F));
            expect(static_cast<bool>(caret_tree.finalize()));

            auto caret_layout = calculate_layout(caret_tree, measure, { 120.F, 40.F });
            expect(static_cast<bool>(caret_layout));
            if (caret_layout)
            {
                const auto *field  = caret_tree.find("field");
                const auto *bounds = caret_layout->find("field");
                expect(field != nullptr);
                expect(bounds != nullptr);
                if (field != nullptr and bounds != nullptr)
                {
                    const auto caret = context.text_input_caret_rect(
                        *field, *bounds, text_selection { .caret = 4, .anchor = 4 });
                    expect(caret.has_value());
                    if (caret)
                        expect(caret->position.x > bounds->position.x + (bounds->size.x / 2.F))
                            << "the right-aligned caret did not move to the right";
                }
            }

            // The real measurer honours wrap_anywhere for unbroken words.
            context.set_viewport({ 64.F, 64.F });

            const auto long_word_tree = [](text_wrap_mode wrap)
            {
                return to_data(box()[label("abcdefghijklmnopqrstuvwxyz") | id("text") | width(64.F)
                                     | text_size(12.F) | text_wrap(wrap)]
                               | size(64.F, 64.F));
            };

            auto wrapped_word_tree = long_word_tree(text_wrap_mode::wrap_anywhere);
            expect(static_cast<bool>(wrapped_word_tree.finalize()));
            auto wrapped_word_layout = calculate_layout(wrapped_word_tree, measure, { 64.F, 64.F });
            expect(static_cast<bool>(wrapped_word_layout));

            auto single_word_tree = long_word_tree(text_wrap_mode::no_wrap);
            expect(static_cast<bool>(single_word_tree.finalize()));
            auto single_word_layout = calculate_layout(single_word_tree, measure, { 64.F, 64.F });
            expect(static_cast<bool>(single_word_layout));

            if (wrapped_word_layout and single_word_layout)
            {
                const auto *wrapped = wrapped_word_layout->find("text");
                const auto *single  = single_word_layout->find("text");
                expect(wrapped != nullptr);
                expect(single != nullptr);
                if (wrapped != nullptr and single != nullptr)
                    expect(wrapped->size.y > single->size.y)
                        << "wrap_anywhere did not split the long word";
            }

            // Explicit radii round the corners of a normal (non-root) node.
            auto rounded
                = to_data(column()[box() | size(64.F) | background(0xFF0000_rgb) | radius(16.F)]
                          | size(64.F, 64.F));
            expect(static_cast<bool>(rounded.finalize()));

            auto rounded_layout = calculate_layout(rounded, measure, { 64.F, 64.F });
            expect(static_cast<bool>(rounded_layout));
            if (rounded_layout)
            {
                expect(static_cast<bool>(context.render(rounded, *rounded_layout)));

                auto *pixels = SDL_RenderReadPixels(renderer, nullptr);
                expect(pixels != nullptr);
                if (pixels != nullptr)
                {
                    std::uint8_t r = 0;
                    std::uint8_t g = 0;
                    std::uint8_t b = 0;
                    std::uint8_t a = 0;
                    expect(SDL_ReadSurfacePixel(pixels, 1, 1, &r, &g, &b, &a));
                    expect(!(r == 0xFF and g == 0x00 and b == 0x00))
                        << "the rounded corner was filled";

                    expect(SDL_ReadSurfacePixel(pixels, 32, 32, &r, &g, &b, &a));
                    expect(r == 0xFF);
                    expect(g == 0x00);
                    expect(b == 0x00);
                    SDL_DestroySurface(pixels);
                }
            }
        }
        catch (...)
        {
            expect(false) << "render_context threw";
        }

        cleanup(window, renderer);
    };
};


auto main() -> int {}
