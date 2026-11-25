#include <string>

#include <boost/ut.hpp>

#include "sleet/widget/theme.hh"


const boost::ut::suite theme_tests = []
{
    using namespace boost::ut;
    using namespace sleet::widget;

    "missing roles inherit the body face"_test = []
    {
        font_config config;
        config.body    = font_face { "body.ttf", 16.F };
        config.heading = {};
        config.code    = font_face { "code.ttf", 14.F };

        expect(config.face(font_role::body).path == "body.ttf");
        expect(config.face(font_role::heading).path == "body.ttf");
        expect(config.face(font_role::caption).path == "body.ttf");
        expect(config.face(font_role::code).path == "code.ttf");
    };

    "default themes provide state colors"_test = []
    {
        const auto dark  = theme::default_dark();
        const auto light = theme::default_light();

        expect(dark.base.bg.has_value());
        expect(dark.base.fg.has_value());
        expect(dark.hover.bg.has_value());
        expect(dark.clicked.bg.has_value());
        expect(dark.border.has_value());
        expect(dark.field.bg.has_value());
        expect(dark.field.fg.has_value());
        expect(dark.focus.has_value());
        expect(dark.accent.has_value());
        expect(dark.accent_fg.has_value());
        expect(dark.danger.has_value());
        expect(dark.danger_fg.has_value());
        expect(dark.caret.has_value());
        expect(dark.selection.has_value());
        expect(dark.muted.has_value());

        expect(light.base.bg.has_value());
        expect(light.base.fg.has_value());
        expect(light.hover.bg.has_value());
        expect(light.clicked.bg.has_value());
        expect(light.border.has_value());
        expect(light.field.bg.has_value());
        expect(light.field.fg.has_value());
        expect(light.focus.has_value());
        expect(light.accent.has_value());
        expect(light.accent_fg.has_value());
        expect(light.danger.has_value());
        expect(light.danger_fg.has_value());
        expect(light.caret.has_value());
        expect(light.selection.has_value());
        expect(light.muted.has_value());
    };
};


auto main() -> int {}
