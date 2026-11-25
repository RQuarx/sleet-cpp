#pragma once
#include <compare>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include <kei-utils/result.hh>
#include <utf8.h>


namespace sleet::txt
{
    /// An owning UTF-8 string. Every mutating operation either keeps the string
    /// valid UTF-8 or returns an error. No operation splits a code point
    /// silently.
    ///
    /// \c insert, \c erase, and \c resize take code point indices. \c size and
    /// \c capacity return byte counts.
    class ustring
    {
    public:
        // The string is always valid UTF-8, so iteration can skip the checked
        // iterator's per-step validation. Callers of from_utf8_unchecked take
        // on the same responsibility.
        using const_iterator = utf8::unchecked::iterator<std::string::const_iterator>;

        static constexpr auto npos = std::numeric_limits<std::size_t>::max();


        ustring() = default;

        /// Builds a \c ustring from a UTF-8 C string and its byte length.
        /// \param string A UTF-8 C string.
        /// \param len The byte length of \p string.
        /// \return The validated string, or an error if the bytes are not valid
        /// UTF-8.
        [[nodiscard]] static auto from_utf8(const char *string, std::size_t len)
            -> kei::result<ustring>;

        [[nodiscard]] static auto from_utf8(std::string string) -> kei::result<ustring>;

        [[nodiscard]] static auto from_utf8(std::string_view string) -> kei::result<ustring>
        { return from_utf8(std::string { string }); }


        [[nodiscard]] auto length() const -> std::size_t;
        [[nodiscard]] auto size() const noexcept -> std::size_t;
        [[nodiscard]] auto capacity() const noexcept -> std::size_t;
        [[nodiscard]] auto empty() const noexcept -> bool;
        [[nodiscard]] auto string() const noexcept -> const std::string &;
        [[nodiscard]] auto c_str() const noexcept -> const char *;
        [[nodiscard]] auto view() const noexcept -> std::string_view;


        void clear() noexcept;
        void reserve(std::size_t new_capacity);
        void shrink_to_fit();

        /// Resizes to \c new_size code points. Growing appends U+0020 spaces.
        /// \param new_size The new length in code points.
        auto resize(std::size_t new_size) -> kei::result<>;

        /// Removes the last code point.
        /// \return An error if the string is empty.
        auto pop_back() -> kei::result<>;

        auto insert(std::size_t index, char32_t cp) -> kei::result<>;
        auto insert(std::size_t index, std::string_view string) -> kei::result<>;
        auto erase(std::size_t index, std::size_t count = 1) -> kei::result<>;
        auto push_back(char32_t cp) -> kei::result<>;

        /// Appends a UTF-8 C string with the given byte length.
        /// \param string A UTF-8 C string.
        /// \param len The byte length of \p string.
        /// \return An error if \p string is null while \p len is nonzero, or if
        /// the bytes are not valid UTF-8.
        auto append(const char *string, std::size_t len) -> kei::result<>;
        auto append(std::string_view string) -> kei::result<>;
        auto append(const std::string &string) -> kei::result<>;


        [[nodiscard]] auto front() const noexcept -> char32_t;
        [[nodiscard]] auto back() const noexcept -> char32_t;
        [[nodiscard]] auto begin() const noexcept -> const_iterator;
        [[nodiscard]] auto end() const noexcept -> const_iterator;


        [[nodiscard]]
        friend auto operator==(const ustring &lhs, const ustring &rhs) noexcept -> bool
        { return lhs.m_string == rhs.m_string; }

        [[nodiscard]]
        friend auto operator<=>(const ustring &lhs, const ustring &rhs) noexcept
            -> std::strong_ordering
        { return lhs.m_string <=> rhs.m_string; }

    private:
        std::string m_string;


        explicit ustring(std::string string) : m_string { std::move(string) } {}


        [[nodiscard]] static auto validate(char32_t cp) -> kei::result<>;
        [[nodiscard]] static auto validate(std::string_view string) -> kei::result<>;

        /// True when \c string points into this ustring's own buffer. In that
        /// case, mutating the buffer needs a copy first because growth may
        /// reallocate it.
        /// \param string The view to check.
        [[nodiscard]] auto aliases(std::string_view string) const noexcept -> bool;

        template <typename It>
        [[nodiscard]]
        static auto append(It it, char32_t cp) -> kei::result<>
        try
        {
            utf8::append(cp, it);
            return {};
        }
        catch (const std::exception &e)
        {
            return kei::error { "Failed to append code point U+{:04X}: {}",
                                static_cast<std::uint32_t>(cp), e.what() }
                .unexpected();
        }
    };


    /// A non-owning UTF-8 view. Use \c from_utf8 to validate the bytes, or
    /// \c from_utf8_unchecked when the caller already guarantees valid UTF-8.
    class ustring_view
    {
    public:
        using const_iterator = utf8::unchecked::iterator<std::string_view::const_iterator>;

        static constexpr auto npos = ustring::npos;


        [[nodiscard]] static auto from_utf8(std::string_view string) -> kei::result<ustring_view>;
        [[nodiscard]] static auto from_utf8_unchecked(std::string_view string) noexcept
            -> ustring_view;


        [[nodiscard]] auto length() const -> std::size_t;
        [[nodiscard]] auto size() const noexcept -> std::size_t;
        [[nodiscard]] auto empty() const noexcept -> bool;
        [[nodiscard]] auto string() const noexcept -> std::string_view;
        [[nodiscard]] auto data() const noexcept -> const char *;
        [[nodiscard]] auto front() const noexcept -> char32_t;
        [[nodiscard]] auto back() const noexcept -> char32_t;
        [[nodiscard]] auto begin() const noexcept -> const_iterator;
        [[nodiscard]] auto end() const noexcept -> const_iterator;


        [[nodiscard]]
        friend auto operator==(ustring_view lhs, ustring_view rhs) noexcept -> bool
        { return lhs.m_string == rhs.m_string; }

        [[nodiscard]]
        friend auto operator<=>(ustring_view lhs, ustring_view rhs) noexcept -> std::strong_ordering
        { return lhs.m_string <=> rhs.m_string; }

    private:
        explicit ustring_view(std::string_view string) noexcept : m_string { string } {}


        std::string_view m_string;
    };
}


template <>
struct std::formatter<sleet::txt::ustring>
{
    template <class ParseContext>
    constexpr auto parse(ParseContext &ctx) const noexcept -> ParseContext::iterator
    { return ctx.begin(); }

    template <class FmtContext>
    auto format(const sleet::txt::ustring &value, FmtContext &ctx) const -> FmtContext::iterator
    { return std::format_to(ctx.out(), "{}", value.string()); }
};


template <>
struct std::formatter<sleet::txt::ustring_view>
{
    template <class ParseContext>
    constexpr auto parse(ParseContext &ctx) const noexcept -> ParseContext::iterator
    { return ctx.begin(); }

    template <class FmtContext>
    auto format(sleet::txt::ustring_view value, FmtContext &ctx) const -> FmtContext::iterator
    { return std::format_to(ctx.out(), "{}", value.string()); }
};
