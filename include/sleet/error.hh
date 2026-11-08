#pragma once
#include <SDL3/SDL_error.h>
#include <kei-utils/result.hh>


namespace sleet
{
    namespace _impl
    {
        struct error final
        {
            template <typename... Args>
            auto operator()(kei::format_string<Args...> fmt, Args &&...args) const
                -> std::unexpected<kei::error>
            {
                return kei::error {
                    fmt.source_location(),
                    "{}: {}",
                    std::format(fmt.format_string(), std::forward<Args>(args)...),
                    SDL_GetError(),
                }
                    .unexpected();
            }
        };
    }


    inline constexpr auto error = _impl::error {};
}
