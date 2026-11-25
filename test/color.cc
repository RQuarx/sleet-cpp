#include <boost/ut.hpp>

#include "sleet/color.hh"


const boost::ut::suite color_tests = []
{
    using namespace boost::ut;
    using namespace sleet;
    using namespace sleet::literals;

    "rgb and rgba constructors use the documented byte order"_test = []
    {
        const auto rgb = color::from_rgb(0x112233);
        expect(rgb.r == 0x11);
        expect(rgb.g == 0x22);
        expect(rgb.b == 0x33);
        expect(rgb.a == 0xFF);

        const auto with_alpha = color::from_rgb(0x112233, 0x44);
        expect(with_alpha.a == 0x44);

        const auto rgba = color::from_rgba(0x11223344);
        expect(rgba.r == 0x11);
        expect(rgba.g == 0x22);
        expect(rgba.b == 0x33);
        expect(rgba.a == 0x44);
    };

    "user defined literals"_test = []
    {
        expect(0x112233_rgb == color::from_rgb(0x112233));
        expect(0x11223344_rgba == color::from_rgba(0x11223344));
    };

    "lerp interpolates every channel"_test = []
    {
        const auto black = color::from_rgba(0x00000000);
        const auto white = color::from_rgba(0xFFFFFFFF);

        expect(black.lerp(white, 0.F) == black);
        expect(black.lerp(white, 1.F) == white);

        const auto middle = black.lerp(white, 0.5F);
        expect(middle.r == 128);
        expect(middle.g == 128);
        expect(middle.b == 128);
        expect(middle.a == 128);

        expect(black.lerp(white, 2.F) == white);
        expect(black.lerp(white, -1.F) == black);
    };

    "transparency predicates"_test = []
    {
        expect(color::from_rgba(0x00000000).is_transparent());
        expect(color::from_rgba(0x000000FF).is_opaque());
        expect(!color::from_rgba(0x00000080).is_transparent());
        expect(!color::from_rgba(0x00000080).is_opaque());
    };
};


auto main() -> int {}
