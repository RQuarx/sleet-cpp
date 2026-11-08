#pragma once
#include <type_traits>
#include <variant>

#include <kei-utils/result.hh>


namespace sleet::trait
{
    namespace _impl
    {
        template <typename Tp>
        struct is_variant : std::false_type
        {
        };

        template <typename... Ts>
        struct is_variant<std::variant<Ts...>> : std::true_type
        {
        };

        template <typename Tp>
        inline constexpr bool is_variant_v = is_variant<Tp>::value;

        template <typename Tp>
        concept variant_type = is_variant_v<std::remove_cv_t<Tp>>;
    }


    template <typename Fn, typename State, typename Message>
    concept boot_fn = requires {
        { Fn() } noexcept -> std::same_as<kei::result<std::pair<State, Message>>>;
    } and _impl::is_variant_v<Message>;


    template <typename Fn, typename State, typename Message>
    concept update_fn = requires(State &state, const Message &message) {
        { Fn(state, message) } noexcept -> std::same_as<kei::result<>>;
    } and _impl::is_variant_v<Message>;


    // template <typename Fn, typename State, typename Scene>
    // concept view_fn = requires(const State &state) {
    //     { Fn(state) } noexcept -> std::derived_from<widget::container>;
    // };
}
