#include <cstdint>
#include <string_view>

#include <SDL3/SDL_keycode.h>
#include <boost/ut.hpp>

#include "sleet/shortcut.hh"


const boost::ut::suite shortcut_tests = []
{
    using namespace boost::ut;

    "key chords parse modifiers and names"_test = []
    {
        const auto chord = sleet::parse_key_chord("Ctrl+Shift+S");
        expect(chord.has_value());
        if (!chord) return;

        expect(eq(chord->key, static_cast<std::uint32_t>(SDLK_S)));
        expect(eq(chord->modifiers,
                  static_cast<std::uint16_t>(SDL_KMOD_CTRL | SDL_KMOD_SHIFT)));
        expect(!chord->repeat);
    };

    "key chord names are case-insensitive and accept aliases"_test = []
    {
        const auto command = sleet::parse_key_chord("cmd+enter");
        expect(command.has_value());
        if (command)
        {
            expect(eq(command->key, static_cast<std::uint32_t>(SDLK_RETURN)));
            expect(eq(command->modifiers, static_cast<std::uint16_t>(SDL_KMOD_GUI)));
        }

        const auto page = sleet::parse_key_chord(" Alt + PageUp ");
        expect(page.has_value());
        if (page)
        {
            expect(eq(page->key, static_cast<std::uint32_t>(SDLK_PAGEUP)));
            expect(eq(page->modifiers, static_cast<std::uint16_t>(SDL_KMOD_ALT)));
        }

        const auto function = sleet::parse_key_chord("f5");
        expect(function.has_value());
        if (function)
            expect(eq(function->key, static_cast<std::uint32_t>(SDLK_F5)));
    };

    "invalid key chords are rejected"_test = []
    {
        expect(!sleet::parse_key_chord(""));
        expect(!sleet::parse_key_chord("   "));
        expect(!sleet::parse_key_chord("Ctrl+"));
        expect(!sleet::parse_key_chord("+S"));
        expect(!sleet::parse_key_chord("Ctrl+NoSuchKey"));
        expect(!sleet::parse_key_chord("A+B"));
    };

    "modifier masks normalize side-specific keys"_test = []
    {
        const auto raw = static_cast<std::uint16_t>(
            SDL_KMOD_LCTRL | SDL_KMOD_RALT | SDL_KMOD_CAPS);

        expect(eq(sleet::shortcut_modifiers(raw),
                  static_cast<std::uint16_t>(SDL_KMOD_CTRL | SDL_KMOD_ALT)));
        expect(eq(sleet::shortcut_modifiers(static_cast<std::uint16_t>(SDL_KMOD_LSHIFT)),
                  static_cast<std::uint16_t>(SDL_KMOD_SHIFT)));
    };

    "key chords compare every field"_test = []
    {
        const auto plain  = sleet::key_chord { static_cast<std::uint32_t>(SDLK_S),
                                               static_cast<std::uint16_t>(SDL_KMOD_CTRL),
                                               false };
        const auto repeat = sleet::key_chord { static_cast<std::uint32_t>(SDLK_S),
                                               static_cast<std::uint16_t>(SDL_KMOD_CTRL),
                                               true };

        const auto same = plain;

        expect(plain == same);
        expect(!(plain == repeat));
    };
};


auto main() -> int {}
