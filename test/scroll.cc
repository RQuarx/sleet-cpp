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
};


auto main() -> int {}
