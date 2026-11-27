#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>


namespace sleet
{
    /// A key and the modifiers that must be held with it.
    ///
    /// Modifier bits use the SDL \c SDL_KMOD_* masks. Left and right variants
    /// are normalized to their generic mask, so \c SDL_KMOD_LCTRL matches
    /// \c SDL_KMOD_CTRL. Caps Lock and Num Lock are ignored.
    struct key_chord
    {
        std::uint32_t key       = 0;
        std::uint16_t modifiers = 0;

        /// Whether the shortcut also fires while the key is repeating.
        bool repeat = false;

        auto operator==(const key_chord &) const noexcept -> bool = default;
    };


    /// A key chord and the action it dispatches.
    struct shortcut_binding
    {
        key_chord   chord;
        std::string action;
    };


    /// Normalizes modifier bits to \c SDL_KMOD_SHIFT, \c SDL_KMOD_CTRL,
    /// \c SDL_KMOD_ALT, and \c SDL_KMOD_GUI. Side-specific and lock bits are
    /// discarded.
    /// \param modifiers The raw modifier mask.
    /// \return The normalized mask.
    [[nodiscard]]
    auto shortcut_modifiers(std::uint16_t modifiers) noexcept -> std::uint16_t;


    /// Parses a human-readable shortcut such as \c "Ctrl+Shift+S",
    /// \c "Cmd+Enter", or \c "F5".
    ///
    /// Modifier names are case-insensitive: \c Ctrl, \c Shift, \c Alt, and
    /// \c Gui, plus aliases such as \c Cmd, \c Meta, \c Super, and \c Option.
    /// Key names accept single characters, \c F1 through \c F24, and common
    /// names such as \c Enter, \c Escape, \c Space, \c PageUp, and
    /// \c PageDown. Printable keys use their unshifted name, so a shifted
    /// symbol is written with \c Shift, for example \c "Ctrl+Shift+=".
    ///
    /// \param text The shortcut text.
    /// \return The parsed chord, or \c std::nullopt when \p text is empty,
    /// has no key, has more than one key, or contains an unknown name.
    [[nodiscard]]
    auto parse_key_chord(std::string_view text) -> std::optional<key_chord>;
}
