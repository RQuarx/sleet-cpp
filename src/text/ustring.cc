#include <functional>
#include <iterator>
#include <string_view>

#include <utf8.h>

#include "sleet/error.hh"
#include "sleet/text/ustring.hh"


using sleet::txt::ustring;
using sleet::txt::ustring_view;


auto ustring::from_utf8(std::string string) -> kei::result<ustring>
{
    if (auto res = validate(string); !res) return std::move(res.error()).unexpected();
    return ustring { std::move(string) };
}


auto ustring::from_utf8(const char *string, std::size_t len) -> kei::result<ustring>
{
    if (string == nullptr and len != 0)
        return sleet::error { "Cannot construct a ustring from a null pointer." }.unexpected();
    return from_utf8(std::string_view { string == nullptr ? "" : string, len });
}


void ustring::clear() noexcept { m_string.clear(); }
void ustring::shrink_to_fit() { m_string.shrink_to_fit(); }
void ustring::reserve(std::size_t new_capacity) { m_string.reserve(new_capacity); }

auto ustring::resize(std::size_t new_size) -> kei::result<>
{
    const auto current = length();

    if (new_size < current)
    {
        auto it = m_string.begin();
        utf8::advance(it, new_size, m_string.end());
        m_string.erase(it, m_string.end());
        return {};
    }

    if (new_size > current) m_string.append(new_size - current, ' ');

    return {};
}

auto ustring::pop_back() -> kei::result<>
{
    if (m_string.empty()) return sleet::error { "Cannot pop from an empty ustring." }.unexpected();

    auto it = m_string.end();
    do
    {
        --it;
    } while (it != m_string.begin() and (static_cast<unsigned char>(*it) & 0xC0U) == 0x80U);

    m_string.erase(it, m_string.end());
    return {};
}


auto ustring::c_str() const noexcept -> const char * { return m_string.c_str(); }
auto ustring::string() const noexcept -> const std::string & { return m_string; }
auto ustring::view() const noexcept -> std::string_view { return m_string; }
auto ustring::empty() const noexcept -> bool { return m_string.empty(); }
auto ustring::capacity() const noexcept -> std::size_t { return m_string.capacity(); }
auto ustring::size() const noexcept -> std::size_t { return m_string.size(); }

auto ustring::length() const -> std::size_t
{ return static_cast<std::size_t>(utf8::distance(m_string.begin(), m_string.end())); }

auto ustring::front() const noexcept -> char32_t
{
    if (m_string.empty()) return 0;
    return utf8::unchecked::peek_next(m_string.begin());
}

auto ustring::back() const noexcept -> char32_t
{
    if (m_string.empty()) return 0;

    auto it = m_string.end();
    return utf8::unchecked::prior(it);
}


auto ustring::push_back(char32_t cp) -> kei::result<>
{
    if (auto res = validate(cp); !res) return std::move(res.error()).unexpected();
    return append(std::back_inserter(m_string), cp);
}

auto ustring::insert(std::size_t index, char32_t cp) -> kei::result<>
{
    if (index > length())
        return sleet::error { "Code point index {} is out of range (length {}).", index, length() }
            .unexpected();

    if (auto res = validate(cp); !res) return std::move(res.error()).unexpected();

    auto it = m_string.begin();
    utf8::advance(it, index, m_string.end());
    return append(std::inserter(m_string, it), cp);
}

auto ustring::insert(std::size_t index, std::string_view string) -> kei::result<>
{
    if (auto res = validate(string); !res) return std::move(res.error()).unexpected();

    const auto current = length();
    if (index > current)
        return sleet::error { "Code point index {} is out of range (length {}).", index, current }
            .unexpected();

    auto it = m_string.begin();
    utf8::advance(it, index, m_string.end());

    if (aliases(string))
    {
        const std::string copy { string };
        m_string.insert(it, copy.begin(), copy.end());
    }
    else
        m_string.insert(it, string.begin(), string.end());

    return {};
}

auto ustring::erase(std::size_t index, std::size_t count) -> kei::result<>
{
    const auto current = length();

    if (index > current or count > current - index)
        return sleet::error { "Cannot erase {} code points at index {} (length {}).", count, index,
                              current }
            .unexpected();

    if (count == 0) return {};

    auto first = m_string.begin();
    utf8::advance(first, index, m_string.end());

    auto last = first;
    utf8::advance(last, count, m_string.end());

    m_string.erase(first, last);
    return {};
}


auto ustring::validate(char32_t cp) -> kei::result<>
{
    if (cp > 0x10FFFF or (cp >= 0xD800 and cp <= 0xDFFF))
        return sleet::error { "U+{:04X} is not a valid Unicode code point.",
                              static_cast<std::uint32_t>(cp) }
            .unexpected();
    return {};
}

auto ustring::validate(std::string_view string) -> kei::result<>
{
    const auto invalid = utf8::find_invalid(string.begin(), string.end());
    if (invalid != string.end())
        return sleet::error { "String contains invalid UTF-8 at byte {}.",
                              static_cast<std::size_t>(invalid - string.begin()) }
            .unexpected();
    return {};
}

auto ustring::aliases(std::string_view string) const noexcept -> bool
{
    if (string.empty()) return false;

    const auto less = std::less<const char *> {};

    const auto *self_begin  = m_string.data();
    const auto *self_end    = self_begin + m_string.size();
    const auto *other_begin = string.data();
    const auto *other_end   = other_begin + string.size();

    return less(other_begin, self_end) and less(self_begin, other_end);
}

auto ustring::append(const char *string, std::size_t len) -> kei::result<>
{
    if (string == nullptr and len != 0)
        return sleet::error { "Cannot append a null string." }.unexpected();

    // Delegate so this overload also gets the self-alias check. A plain
    // m_string.append(ptr, len) can invalidate ptr during reallocation.
    return append(std::string_view { string == nullptr ? "" : string, len });
}

auto ustring::append(std::string_view string) -> kei::result<>
{
    if (auto res = validate(string); !res) return std::move(res.error()).unexpected();

    if (aliases(string))
    {
        const std::string copy { string };
        m_string.append(copy);
    }
    else
        m_string.append(string);

    return {};
}

auto ustring::append(const std::string &string) -> kei::result<>
{ return append(std::string_view { string }); }


auto ustring::begin() const noexcept -> const_iterator
{ return const_iterator { m_string.begin() }; }

auto ustring::end() const noexcept -> const_iterator { return const_iterator { m_string.end() }; }


auto ustring_view::from_utf8(std::string_view string) -> kei::result<ustring_view>
{
    const auto invalid = utf8::find_invalid(string.begin(), string.end());
    if (invalid != string.end())
        return sleet::error { "String contains invalid UTF-8 at byte {}.",
                              static_cast<std::size_t>(invalid - string.begin()) }
            .unexpected();
    return ustring_view { string };
}

auto ustring_view::from_utf8_unchecked(std::string_view string) noexcept -> ustring_view
{ return ustring_view { string }; }


auto ustring_view::data() const noexcept -> const char * { return m_string.data(); }
auto ustring_view::string() const noexcept -> std::string_view { return m_string; }
auto ustring_view::empty() const noexcept -> bool { return m_string.empty(); }
auto ustring_view::size() const noexcept -> std::size_t { return m_string.size(); }

auto ustring_view::length() const -> std::size_t
{ return static_cast<std::size_t>(utf8::distance(m_string.begin(), m_string.end())); }

auto ustring_view::front() const noexcept -> char32_t
{
    if (m_string.empty()) return 0;
    return utf8::unchecked::peek_next(m_string.begin());
}

auto ustring_view::back() const noexcept -> char32_t
{
    if (m_string.empty()) return 0;

    auto it = m_string.end();
    return utf8::unchecked::prior(it);
}


auto ustring_view::begin() const noexcept -> const_iterator
{ return const_iterator { m_string.begin() }; }

auto ustring_view::end() const noexcept -> const_iterator
{ return const_iterator { m_string.end() }; }
