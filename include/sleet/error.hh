#pragma once
#include <format>
#include <utility>

#include <SDL3/SDL_error.h>
#include <kei-utils/result.hh>


namespace sleet
{
    /// A framework error. The message is formatted at the call site. Call
    /// \c unexpected() to return it from a \c kei::result.
    class error : public kei::error
    {
    public:
        template <typename... Args>
        error(kei::format_string<Args...> fmt, Args &&...args)
            : kei::error { fmt, std::forward<Args>(args)... }
        {
        }
    };


    /// An error that appends the current SDL error message.
    class sdl_error : public kei::error
    {
    public:
        template <typename... Args>
        sdl_error(kei::format_string<Args...> fmt, Args &&...args)
            : kei::error {
                  fmt.source_location(),
                  "{}: {}",
                  std::format(fmt.format_string(), std::forward<Args>(args)...),
                  SDL_GetError(),
              }
        {
        }
    };
}
