#pragma once
#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>

#include <kei-utils/result.hh>


namespace sleet::trait
{
    /// A function that produces the initial state and the first message.
    template <typename Fn, typename State, typename Message>
    concept boot_fn
        = std::invocable<Fn>
      and std::same_as<std::invoke_result_t<Fn>, kei::result<std::pair<State, Message>>>;


    /// A function that applies a message to the state and may emit more messages.
    template <typename Fn, typename State, typename Message, typename Emitter>
    concept update_fn
        = std::invocable<Fn, State &, const Message &, const Emitter &>
      and std::same_as<std::invoke_result_t<Fn, State &, const Message &, const Emitter &>,
                       kei::result<>>;


    /// A function that maps a framework event to an optional message.
    template <typename Fn, typename Event, typename Message>
    concept event_fn
        = std::invocable<Fn, const Event &>
      and std::convertible_to<std::invoke_result_t<Fn, const Event &>, std::optional<Message>>;


    /// A function polled by the runtime to produce messages from external sources.
    template <typename Fn, typename Message>
    concept subscription_fn = std::invocable<Fn>
                          and std::convertible_to<std::invoke_result_t<Fn>, std::optional<Message>>;
}
