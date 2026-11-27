#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>

#include "sleet/shortcut.hh"


namespace
{
    struct named_modifier
    {
        std::string_view name;
        std::uint16_t    mask;
    };


    struct named_key
    {
        std::string_view name;
        std::uint32_t    key;
    };


    constexpr auto modifier_names = std::to_array<named_modifier>({
        { "ctrl", SDL_KMOD_CTRL },
        { "control", SDL_KMOD_CTRL },
        { "shift", SDL_KMOD_SHIFT },
        { "alt", SDL_KMOD_ALT },
        { "option", SDL_KMOD_ALT },
        { "opt", SDL_KMOD_ALT },
        { "gui", SDL_KMOD_GUI },
        { "cmd", SDL_KMOD_GUI },
        { "command", SDL_KMOD_GUI },
        { "meta", SDL_KMOD_GUI },
        { "super", SDL_KMOD_GUI },
        { "win", SDL_KMOD_GUI },
        { "windows", SDL_KMOD_GUI },
    });


    constexpr auto key_names = std::to_array<named_key>({
        { "return", SDLK_RETURN },
        { "enter", SDLK_RETURN },
        { "escape", SDLK_ESCAPE },
        { "esc", SDLK_ESCAPE },
        { "space", SDLK_SPACE },
        { "spacebar", SDLK_SPACE },
        { "tab", SDLK_TAB },
        { "backspace", SDLK_BACKSPACE },
        { "bksp", SDLK_BACKSPACE },
        { "delete", SDLK_DELETE },
        { "del", SDLK_DELETE },
        { "insert", SDLK_INSERT },
        { "ins", SDLK_INSERT },
        { "home", SDLK_HOME },
        { "end", SDLK_END },
        { "pageup", SDLK_PAGEUP },
        { "page_up", SDLK_PAGEUP },
        { "pgup", SDLK_PAGEUP },
        { "pagedown", SDLK_PAGEDOWN },
        { "page_down", SDLK_PAGEDOWN },
        { "pgdn", SDLK_PAGEDOWN },
        { "left", SDLK_LEFT },
        { "right", SDLK_RIGHT },
        { "up", SDLK_UP },
        { "down", SDLK_DOWN },
        { "plus", SDLK_PLUS },
        { "minus", SDLK_MINUS },
        { "equals", SDLK_EQUALS },
        { "equal", SDLK_EQUALS },
        { "comma", SDLK_COMMA },
        { "period", SDLK_PERIOD },
        { "slash", SDLK_SLASH },
        { "backslash", SDLK_BACKSLASH },
        { "semicolon", SDLK_SEMICOLON },
        { "apostrophe", SDLK_APOSTROPHE },
        { "quote", SDLK_APOSTROPHE },
        { "grave", SDLK_GRAVE },
        { "backtick", SDLK_GRAVE },
        { "leftbracket", SDLK_LEFTBRACKET },
        { "rightbracket", SDLK_RIGHTBRACKET },
        { "printscreen", SDLK_PRINTSCREEN },
        { "scrolllock", SDLK_SCROLLLOCK },
        { "pause", SDLK_PAUSE },
        { "menu", SDLK_MENU },
        { "capslock", SDLK_CAPSLOCK },
        { "numlock", SDLK_NUMLOCKCLEAR },
        { "clear", SDLK_CLEAR },
        { "help", SDLK_HELP },
        { "kp_enter", SDLK_KP_ENTER },
        { "keypadenter", SDLK_KP_ENTER },
        { "numpadenter", SDLK_KP_ENTER },
    });


    [[nodiscard]]
    constexpr auto is_space(char value) noexcept -> bool
    {
        return value == ' ' or value == '\t' or value == '\n' or value == '\r';
    }


    [[nodiscard]]
    auto trim(std::string_view text) noexcept -> std::string_view
    {
        while (!text.empty() and is_space(text.front())) text.remove_prefix(1);
        while (!text.empty() and is_space(text.back())) text.remove_suffix(1);
        return text;
    }


    [[nodiscard]]
    auto ascii_lower(std::string_view text) -> std::string
    {
        auto result = std::string {};
        result.reserve(text.size());

        for (const auto value : text)
            result.push_back(
                static_cast<char>(std::tolower(static_cast<unsigned char>(value))));

        return result;
    }


    [[nodiscard]]
    auto modifier_mask(std::string_view name) noexcept -> std::uint16_t
    {
        for (const auto &modifier : modifier_names)
            if (modifier.name == name) return modifier.mask;

        return 0;
    }


    [[nodiscard]]
    auto key_from_token(const std::string &token) noexcept -> std::uint32_t
    {
        for (const auto &named : key_names)
            if (named.name == token) return named.key;

        // SDL resolves single-character names and capitalizes them itself.
        if (token.size() == 1)
            return static_cast<std::uint32_t>(SDL_GetKeyFromName(token.c_str()));

        // Function keys are written F1 through F24. Accept the lower-case form
        // from the parser and hand SDL the canonical spelling.
        if (token.size() >= 2 and token.front() == 'f'
            and std::all_of(token.begin() + 1, token.end(),
                            [](char value) { return value >= '0' and value <= '9'; }))
        {
            auto canonical = token;
            canonical.front() = 'F';
            return static_cast<std::uint32_t>(SDL_GetKeyFromName(canonical.c_str()));
        }

        return static_cast<std::uint32_t>(SDL_GetKeyFromName(token.c_str()));
    }
}


auto sleet::shortcut_modifiers(std::uint16_t modifiers) noexcept -> std::uint16_t
{
    auto result = std::uint16_t { 0 };

    if ((modifiers & SDL_KMOD_SHIFT) != 0)
        result = static_cast<std::uint16_t>(result | SDL_KMOD_SHIFT);
    if ((modifiers & SDL_KMOD_CTRL) != 0)
        result = static_cast<std::uint16_t>(result | SDL_KMOD_CTRL);
    if ((modifiers & SDL_KMOD_ALT) != 0)
        result = static_cast<std::uint16_t>(result | SDL_KMOD_ALT);
    if ((modifiers & SDL_KMOD_GUI) != 0)
        result = static_cast<std::uint16_t>(result | SDL_KMOD_GUI);

    return result;
}


auto sleet::parse_key_chord(std::string_view text) -> std::optional<key_chord>
{
    text = trim(text);
    if (text.empty()) return std::nullopt;

    auto modifiers = std::uint16_t { 0 };
    auto key       = std::uint32_t { 0 };
    auto have_key  = false;

    while (true)
    {
        const auto separator = text.find('+');
        const auto token
            = trim(separator == std::string_view::npos ? text : text.substr(0, separator));
        if (token.empty()) return std::nullopt;

        const auto lower = ascii_lower(token);
        if (const auto mask = modifier_mask(lower); mask != 0)
            modifiers = static_cast<std::uint16_t>(modifiers | mask);
        else
        {
            if (have_key) return std::nullopt;

            key = key_from_token(lower);
            if (key == 0) return std::nullopt;
            have_key = true;
        }

        if (separator == std::string_view::npos) break;
        text.remove_prefix(separator + 1);
    }

    if (!have_key) return std::nullopt;
    return key_chord { key, shortcut_modifiers(modifiers), false };
}
