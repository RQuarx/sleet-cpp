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

            const auto measure = [&context](std::string_view text, const font_spec &font,
                                            std::optional<float> width_limit)
            { return context.measure_text(text, font, width_limit); };

            auto layout = calculate_layout(tree, measure, { 64.F, 64.F });
            expect(static_cast<bool>(layout));
            if (!layout)
            {
                cleanup(window, renderer);
                return;
            }

            expect(static_cast<bool>(context.render(tree, *layout)));

            auto *pixels = SDL_RenderReadPixels(renderer, nullptr);
            expect(pixels != nullptr);
            if (pixels != nullptr)
            {
                std::uint8_t r = 0;
                std::uint8_t g = 0;
                std::uint8_t b = 0;
                std::uint8_t a = 0;
                expect(SDL_ReadSurfacePixel(pixels, 32, 32, &r, &g, &b, &a));
                expect(r == 0xFF);
                expect(g == 0x00);
                expect(b == 0x00);
                expect(a == 0xFF);
                SDL_DestroySurface(pixels);
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
