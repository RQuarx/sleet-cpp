#include <boost/ut.hpp>

#include "sleet/widget/animation.hh"


const boost::ut::suite animation_tests = []
{
    using namespace boost::ut;
    using namespace sleet::widget;

    "easing curves pin both endpoints"_test = []
    {
        for (const auto curve : { easing::linear, easing::ease_in, easing::ease_out,
                                  easing::ease_in_out, easing::ease_out_back })
        {
            expect(eq(ease(curve, -1.F), 0.F));
            expect(eq(ease(curve, 0.F), 0.F));
            expect(eq(ease(curve, 1.F), 1.F));
            expect(eq(ease(curve, 2.F), 1.F));
        }
    };

    "easing curves advance monotonically"_test = []
    {
        for (const auto curve :
             { easing::linear, easing::ease_in, easing::ease_out, easing::ease_in_out })
        {
            auto previous = 0.F;
            for (auto step = 0; step <= 20; step++)
            {
                const auto value = ease(curve, static_cast<float>(step) / 20.F);
                expect(value >= previous) << "curve moved backwards";
                previous = value;
            }
        }
    };

    "animated values ease toward their target"_test = []
    {
        animated_value value { 0.F };
        expect(eq(value.current(), 0.F));
        expect(value.settled());

        value.set_target(1.F);
        expect(!value.settled());

        expect(value.advance(0.5F, 1.F, easing::linear));
        expect(eq(value.current(), 0.5F));

        expect(value.advance(0.5F, 1.F, easing::linear));
        expect(eq(value.current(), 1.F));
        expect(value.settled());
        expect(!value.advance(1.F, 1.F, easing::linear));
    };

    "a zero duration snaps to the target"_test = []
    {
        animated_value value { 0.F };
        value.set_target(1.F);

        expect(!value.advance(0.016F, 0.F, easing::ease_out));
        expect(eq(value.current(), 1.F));
    };

    "changing the target restarts from the visible value"_test = []
    {
        animated_value value { 0.F };
        value.set_target(1.F);
        expect(value.advance(0.5F, 1.F, easing::linear));
        expect(eq(value.current(), 0.5F));

        value.set_target(0.F);
        expect(value.advance(0.25F, 1.F, easing::linear));
        expect(eq(value.current(), 0.375F));
    };
};


auto main() -> int {}
