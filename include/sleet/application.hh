#pragma once
#include <SDL3/SDL_init.h>
#include <kei-utils/runtime_guard.hh>

#include "error.hh"


namespace sleet
{
    namespace _impl
    {
        struct runtime_policy
        {
            static auto init() noexcept -> kei::result<>
            {
                if (!SDL_Init(SDL_INIT_VIDEO)) return sleet::error("Failed to initialize SDL");
                return {};
            }


            static void deinit() noexcept {}
        };


        static_assert(kei::runtime_policy<runtime_policy>);
    }


    class application final : public kei::runtime_guard<_impl::runtime_policy>
    {
    };
}
