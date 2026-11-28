#include <cmath>

#include <boost/ut.hpp>

#include "sleet/widget/scroll.hh"


const boost::ut::suite scroll_tests = []
{
    using namespace boost::ut;
    using namespace sleet::widget;

    "a thumb fills the track when content fits"_test = []
    {
        const auto thumb = scrollbar_thumb_geometry(100.F, 80.F, 0.F);
        expect(eq(thumb.length, 0.F));
    };

    "a thumb shrinks with the content ratio"_test = []
    {
        const auto thumb = scrollbar_thumb_geometry(100.F, 200.F, 0.F, 10.F);
        expect(eq(thumb.length, 50.F));
        expect(eq(thumb.start, 0.F));
    };

    "a thumb travels to the end at maximum offset"_test = []
    {
        const auto thumb = scrollbar_thumb_geometry(100.F, 200.F, 100.F, 10.F);
        expect(eq(thumb.length, 50.F));
        expect(eq(thumb.start, 50.F));
    };

    "a thumb keeps its minimum length"_test = []
    {
        const auto thumb = scrollbar_thumb_geometry(100.F, 1000.F, 0.F, 24.F);
        expect(eq(thumb.length, 24.F));
        expect(thumb.start == 0.F);
    };

    "offsets outside the content are clamped"_test = []
    {
        const auto thumb = scrollbar_thumb_geometry(100.F, 200.F, -50.F, 10.F);
        expect(eq(thumb.start, 0.F));

        const auto past = scrollbar_thumb_geometry(100.F, 200.F, 500.F, 10.F);
        expect(eq(past.start, 50.F));
    };

    "vertical scrollbar geometry matches the painted track"_test = []
    {
        const auto bars = scrollbar_layout_for(
            sleet::math::rect {
                0.F, 0.F, 100.F, 100.F
        },
            scroll_metrics { .offset = { 0.F, 0.F }, .max = { 0.F, 100.F }, .viewport = {} });

        expect(bars.vertical.visible);
        expect(!bars.horizontal.visible);
        expect(eq(bars.vertical.track.position.x, 92.F));
        expect(eq(bars.vertical.track.position.y, 2.F));
        expect(eq(bars.vertical.track.size.x, 6.F));
        expect(eq(bars.vertical.track.size.y, 96.F));
        expect(std::abs(bars.vertical.thumb.size.y - (96.F * 96.F / 196.F)) < 0.001F);
        expect(eq(bars.vertical.thumb.position.y, 2.F));
    };

    "horizontal scrollbar geometry leaves room for the vertical bar"_test = []
    {
        const auto bars = scrollbar_layout_for(
            sleet::math::rect {
                0.F, 0.F, 100.F, 100.F
        },
            scroll_metrics { .offset = { 0.F, 0.F }, .max = { 100.F, 100.F }, .viewport = {} });

        expect(bars.vertical.visible);
        expect(bars.horizontal.visible);
        expect(eq(bars.horizontal.track.position.x, 2.F));
        expect(eq(bars.horizontal.track.position.y, 92.F));
        expect(eq(bars.horizontal.track.size.x, 88.F));
        expect(eq(bars.horizontal.track.size.y, 6.F));
    };

    "scrollbar thumbs follow the offset along each axis"_test = []
    {
        const auto bars = scrollbar_layout_for(
            sleet::math::rect {
                0.F, 0.F, 100.F, 100.F
        },
            scroll_metrics { .offset = { 100.F, 100.F }, .max = { 100.F, 100.F }, .viewport = {} });

        expect(std::abs(bars.vertical.thumb.position.y + bars.vertical.thumb.size.y
                        - bars.vertical.track.bottom())
               < 0.001F);
        expect(std::abs(bars.horizontal.thumb.position.x + bars.horizontal.thumb.size.x
                        - bars.horizontal.track.right())
               < 0.001F);
    };

    "a viewport without overflow has no scrollbars"_test = []
    {
        const auto bars
            = scrollbar_layout_for(sleet::math::rect { 0.F, 0.F, 100.F, 100.F },
                                   scroll_metrics { .offset = {}, .max = {}, .viewport = {} });

        expect(!bars.vertical.visible);
        expect(!bars.horizontal.visible);
    };
};


auto main() -> int {}
