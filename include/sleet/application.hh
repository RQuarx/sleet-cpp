#pragma once
#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <kei-utils/runtime_guard.hh>

#include "sleet/error.hh"
#include "sleet/render_context.hh"
#include "sleet/traits.hh"
#include "sleet/widget/control.hh"
#include "sleet/widget/layout.hh"
#include "sleet/widget/node.hh"
#include "sleet/widget/theme.hh"


namespace sleet
{
    /// An input event from the framework. It carries the hit-tested widget and
    /// the nearest \c on_click action, which lets an Elm-style \c update turn
    /// it into a message.
    ///
    /// Control widgets emit their own event kinds. A text input reports
    /// \c text_edit and \c submit, a combo box reports \c selection_changed, a
    /// slider reports \c slider_changed, and keyboard activation of a button,
    /// checkbox, toggle, or radio button reports \c activate.
    struct event
    {
        enum class kind : std::uint8_t
        {
            quit,
            resize,
            mouse_move,
            mouse_down,
            mouse_up,
            scroll,
            key_down,
            key_up,
            text_input,
            text_edit,
            submit,
            selection_changed,
            slider_changed,
            activate,
        };


        kind              type = kind::quit;
        widget::widget_id target;
        std::string       action;

        float x        = 0.F;
        float y        = 0.F;
        float scroll_x = 0.F;
        float scroll_y = 0.F;

        std::uint32_t key       = 0;
        std::uint8_t  button    = 0;
        std::uint16_t modifiers = 0;
        bool          repeat    = false;
        std::string   text;

        /// A normalized control value in the range zero to one. Sliders use it.
        float value = 0.F;

        int width  = 0;
        int height = 0;
    };


    namespace _impl
    {
        /// \c kei::runtime_policy requires \c init() to be \c noexcept, but
        /// formatting an error message allocates. If that allocation fails while
        /// reporting a failed initialization, there is nowhere left to report it.
        [[nodiscard]]
        inline auto runtime_init_error(const char *message) noexcept -> kei::result<>
        {
            try
            {
                return sleet::sdl_error { "{}", message }.unexpected();
            }
            catch (...)
            {
                std::terminate();
            }
        }


        struct runtime_policy
        {
            static auto init() noexcept -> kei::result<>
            {
                if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
                    return runtime_init_error("Failed to initialize SDL");
                return {};
            }

            static void deinit() noexcept { SDL_Quit(); }
        };


        struct ttf_runtime_policy
        {
            static auto init() noexcept -> kei::result<>
            {
                if (!TTF_Init()) return runtime_init_error("Failed to initialize SDL_ttf");
                return {};
            }

            static void deinit() noexcept { TTF_Quit(); }
        };


        static_assert(kei::runtime_policy<runtime_policy>);
        static_assert(kei::runtime_policy<ttf_runtime_policy>);

        struct window_deleter
        {
            void operator()(SDL_Window *window) const noexcept
            {
                if (window != nullptr) SDL_DestroyWindow(window);
            }
        };

        struct renderer_deleter
        {
            void operator()(SDL_Renderer *renderer) const noexcept
            {
                if (renderer != nullptr) SDL_DestroyRenderer(renderer);
            }
        };


        /// Keeps SDL text input enabled for as long as this guard lives.
        struct text_input_guard
        {
            SDL_Window *window = nullptr;


            explicit text_input_guard(SDL_Window *target) noexcept : window { target }
            {
                if (window != nullptr) SDL_StartTextInput(window);
            }

            ~text_input_guard()
            {
                if (window != nullptr) SDL_StopTextInput(window);
            }

            text_input_guard(const text_input_guard &)            = delete;
            text_input_guard &operator=(const text_input_guard &) = delete;
        };


        [[nodiscard]]
        inline auto action_target(const widget::node_data *node) noexcept
            -> const widget::node_data *
        {
            for (auto *current = node; current != nullptr; current = current->parent)
                if (current->control == widget::control_kind::none and !current->action.empty())
                    return current;
            return nullptr;
        }


        [[nodiscard]]
        inline auto control_target(const widget::node_data *node) noexcept
            -> const widget::node_data *
        {
            for (auto *current = node; current != nullptr; current = current->parent)
                if (current->control != widget::control_kind::none) return current;
            return nullptr;
        }


        [[nodiscard]]
        inline auto focus_target(const widget::node_data *node) noexcept
            -> const widget::node_data *
        {
            for (auto *current = node; current != nullptr; current = current->parent)
                if (current->focusable) return current;
            return nullptr;
        }


        [[nodiscard]]
        inline auto interactive_target(const widget::node_data *node) noexcept
            -> const widget::node_data *
        {
            for (auto *current = node; current != nullptr; current = current->parent)
            {
                // Indicators are visual pieces of their parent control. Treating
                // them as separate targets would break press/release matching
                // and hide the parent's hover feedback.
                if (current->control == widget::control_kind::checkbox_indicator
                    or current->control == widget::control_kind::radio_indicator)
                    continue;

                if (!current->action.empty() or current->control != widget::control_kind::none)
                    return current;
            }
            return nullptr;
        }


        [[nodiscard]]
        inline auto hit_test(const widget::node_data     &node,
                             const widget::layout_result &layout,
                             math::vec2 point) noexcept -> const widget::node_data *
        {
            if (!node.visible) return nullptr;

            const auto *bounds = layout.find(node.id);
            if (bounds == nullptr or !bounds->contains(point)) return nullptr;

            for (auto it = node.children.rbegin(); it != node.children.rend(); ++it)
                if (auto *hit = hit_test(*it, layout, point); hit != nullptr) return hit;

            return &node;
        }
    }


    /// A compact Elm-style application runtime built on SDL3 and SDL3_ttf.
    ///
    /// \c Boot returns the initial state and a message. \c Update handles
    /// messages, and \c View turns the state into a widget tree. An optional
    /// event mapper and subscription callback let the application receive
    /// input and messages from outside the runtime.
    template <typename State, typename Message>
    class application final : private kei::runtime_guard<_impl::runtime_policy>,
                              private kei::runtime_guard<_impl::ttf_runtime_policy>
    {
    public:
        using message_emitter = std::function<void(Message)>;
        using update_fn
            = std::function<auto(State &, const Message &, const message_emitter &)->kei::result<>>;
        using boot_fn         = std::function<auto()->kei::result<std::pair<State, Message>>>;
        using view_fn         = std::function<auto(const State &)->kei::result<widget::node_data>>;
        using event_fn        = std::function<auto(const event &)->std::optional<Message>>;
        using subscription_fn = std::function<auto()->std::optional<Message>>;


        application(const std::string &title,
                    int                width  = 800,
                    int                height = 600,
                    widget::theme      theme  = widget::theme::default_dark())
            : m_width { width }, m_height { height }, m_theme { std::move(theme) }
        {
            m_window.reset(SDL_CreateWindow(title.c_str(), width, height,
                                            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY));
            if (!m_window)
                throw kei::error { "Failed to create the application window: {}", SDL_GetError() };

            SDL_GetWindowSize(m_window.get(), &m_width, &m_height);

            m_renderer.reset(SDL_CreateRenderer(m_window.get(), nullptr));
            if (!m_renderer)
                throw kei::error { "Failed to create the renderer: {}", SDL_GetError() };

            SDL_SetRenderVSync(m_renderer.get(), 1);
            SDL_SetRenderLogicalPresentation(m_renderer.get(), m_width, m_height,
                                             SDL_LOGICAL_PRESENTATION_LETTERBOX);
        }

        ~application() = default;

        application(const application &)            = delete;
        application &operator=(const application &) = delete;
        application(application &&)                 = delete;
        application &operator=(application &&)      = delete;


        template <typename Fn>
            requires trait::boot_fn<Fn, State, Message>
        auto on_boot(Fn &&fn) -> application &
        {
            m_boot = [fn = std::forward<Fn>(fn)]() mutable { return fn(); };
            return *this;
        }

        template <typename Fn>
            requires trait::update_fn<Fn, State, Message, message_emitter>
        auto on_update(Fn &&fn) -> application &
        {
            m_update = [fn = std::forward<Fn>(fn)](State &state, const Message &message,
                                                   const message_emitter &emit) mutable
            { return fn(state, message, emit); };
            return *this;
        }

        template <typename Fn>
            requires(
                std::invocable<Fn, State &, const Message &>
                and std::same_as<std::invoke_result_t<Fn, State &, const Message &>, kei::result<>>
                and !std::invocable<Fn, State &, const Message &, const message_emitter &>)
        auto on_update(Fn &&fn) -> application &
        {
            m_update = [fn = std::forward<Fn>(fn)](State &state, const Message &message,
                                                   const message_emitter &) mutable
            { return fn(state, message); };
            return *this;
        }

        template <typename Fn>
            requires std::invocable<Fn, const State &>
                 and widget::_impl::widget_like<std::invoke_result_t<Fn, const State &>>
        auto on_view(Fn &&fn) -> application &
        {
            m_view = [fn = std::forward<Fn>(fn)](
                         const State &state) mutable -> kei::result<widget::node_data>
            { return widget::to_data(fn(state)); };
            return *this;
        }

        template <typename Fn>
            requires trait::event_fn<Fn, event, Message>
        auto on_event(Fn &&fn) -> application &
        {
            m_event = [fn = std::forward<Fn>(fn)](const event &value) mutable { return fn(value); };
            return *this;
        }

        template <typename Fn>
            requires trait::subscription_fn<Fn, Message>
        auto on_subscription(Fn &&fn) -> application &
        {
            m_subscription = [fn = std::forward<Fn>(fn)]() mutable { return fn(); };
            return *this;
        }

        void set_theme(widget::theme theme) { m_theme = std::move(theme); }


        auto run() -> int
        {
            if (!m_boot or !m_update or !m_view)
            {
                std::println(stderr, "sleet: boot, update, and view callbacks are required.");
                return 1;
            }

            try
            {
                return run_impl();
            }
            catch (const std::exception &error)
            {
                std::println(stderr, "sleet: {}", error.what());
                return 1;
            }
        }

    private:
        std::unique_ptr<SDL_Window, _impl::window_deleter>     m_window;
        std::unique_ptr<SDL_Renderer, _impl::renderer_deleter> m_renderer;

        int           m_width  = 800;
        int           m_height = 600;
        widget::theme m_theme;

        boot_fn         m_boot;
        update_fn       m_update;
        view_fn         m_view;
        event_fn        m_event;
        subscription_fn m_subscription;


        struct session
        {
            explicit session(State initial) : state { std::move(initial) } {}

            State                                state;
            std::deque<Message>                  messages;
            std::optional<widget::node_data>     tree;
            std::optional<widget::layout_result> layout;
            std::optional<widget::widget_id>     hovered;
            std::optional<widget::widget_id>     pressed;
            std::optional<widget::widget_id>     focused;
            std::optional<widget::widget_id>     selecting;
            std::optional<widget::widget_id>     dragging;

            std::unordered_map<widget::widget_id, widget::text_selection> edits;
            std::unordered_set<widget::widget_id>                         open_combos;

            bool press_active   = false;
            bool quit_requested = false;
            bool dirty          = true;
        };


        [[nodiscard]]
        auto to_logical(float window_x, float window_y) const noexcept -> math::vec2
        {
            float x = window_x;
            float y = window_y;
            if (!SDL_RenderCoordinatesFromWindow(m_renderer.get(), window_x, window_y, &x, &y))
                return { window_x, window_y };
            return { x, y };
        }


        [[nodiscard]]
        auto hit_at(const session &current, float window_x, float window_y) const
            -> const widget::node_data *
        {
            if (!current.tree or !current.layout) return nullptr;
            return _impl::hit_test(*current.tree, *current.layout, to_logical(window_x, window_y));
        }


        [[nodiscard]]
        auto focused_node(session &current) const -> widget::node_data *
        {
            if (!current.tree or !current.focused) return nullptr;
            return current.tree->find(*current.focused);
        }


        // Theme hover and press colors belong to interactive widgets. Plain
        // widgets still report a hit target, but hovering them should not
        // change their color.
        [[nodiscard]]
        static auto visual_target(const widget::node_data *hit) -> std::optional<widget::widget_id>
        {
            if (const auto *target = _impl::interactive_target(hit); target != nullptr)
                return target->id;
            return std::nullopt;
        }


        [[nodiscard]]
        static auto make_event(event::kind type, const widget::node_data *hit) -> event
        {
            event value;
            value.type = type;

            if (const auto *control = _impl::control_target(hit); control != nullptr)
                value.target = control->id;
            else if (hit != nullptr)
                value.target = hit->id;

            if (const auto *target = _impl::action_target(hit); target != nullptr)
                value.action = target->action;

            return value;
        }


        void dispatch(const event &value, session &current) const
        {
            if (!m_event) return;
            if (auto message = m_event(value)) current.messages.push_back(std::move(*message));
        }


        void emit_control(session                 &current,
                          event::kind              type,
                          const widget::node_data &node,
                          std::string              text  = {},
                          float                    value = 0.F) const
        {
            event result;
            result.type   = type;
            result.target = node.id;
            result.action = node.action;
            result.text   = std::move(text);
            result.value  = value;
            dispatch(result, current);
        }


        static void clear_pointer_state(session &current)
        {
            if (!current.hovered and !current.pressed and !current.press_active) return;

            current.hovered.reset();
            current.pressed.reset();
            current.press_active = false;
            current.dirty        = true;
        }


        static void update_press_state(session &current, const widget::node_data *hit, bool down)
        {
            if (down)
            {
                current.press_active = true;
                current.pressed      = visual_target(hit);
                current.hovered      = current.pressed;
            }
            else
            {
                current.press_active = false;
                current.pressed.reset();
                current.hovered = visual_target(hit);
            }

            current.dirty = true;
        }


        // A release activates an action only if the press began on that same
        // action. The release still reports its own hit target for general
        // handling.
        [[nodiscard]]
        static auto release_matches_press(const session &current, const widget::node_data *hit)
            -> bool
        {
            const auto *released = _impl::action_target(hit);
            return current.press_active and current.pressed and released != nullptr
               and released->id == *current.pressed;
        }


        static void collect_focusable(const widget::node_data                &node,
                                      std::vector<const widget::node_data *> &result)
        {
            if (!node.visible) return;
            if (node.focusable) result.push_back(&node);
            for (const auto &child : node.children) collect_focusable(child, result);
        }


        void focus_node(session &current, const widget::node_data &node)
        {
            if (current.focused and *current.focused == node.id) return;

            current.focused = node.id;
            if (node.control == widget::control_kind::text_input)
                current.edits[node.id].collapse(node.text.size());
            current.dirty = true;
        }


        void focus_next(session &current, bool backward)
        {
            if (!current.tree) return;

            std::vector<const widget::node_data *> nodes;
            collect_focusable(*current.tree, nodes);

            if (nodes.empty())
            {
                current.focused.reset();
                current.dirty = true;
                return;
            }

            const auto it
                = std::find_if(nodes.begin(), nodes.end(), [&current](const auto *node)
                               { return current.focused and node->id == *current.focused; });

            auto index = std::size_t { 0 };
            if (it == nodes.end())
                index = backward ? nodes.size() - 1 : 0;
            else if (backward)
                index = ((static_cast<std::size_t>(it - nodes.begin()) + nodes.size()) - 1)
                      % nodes.size();
            else
                index = (static_cast<std::size_t>(it - nodes.begin()) + 1) % nodes.size();

            focus_node(current, *nodes[index]);
        }


        static void apply_combo_visibility(widget::node_data                           &node,
                                           const std::unordered_set<widget::widget_id> &open)
        {
            if (node.control == widget::control_kind::combo_box and node.parent != nullptr)
            {
                const bool is_open = open.contains(node.id);
                for (auto &sibling : node.parent->children)
                    if (sibling.control == widget::control_kind::combo_option)
                        sibling.visible = is_open;
            }

            for (auto &child : node.children) apply_combo_visibility(child, open);
        }


        void sync_controls(session &current, widget::node_data &root)
        {
            apply_combo_visibility(root, current.open_combos);

            for (auto it = current.open_combos.begin(); it != current.open_combos.end();)
            {
                const auto *node = root.find(*it);
                if (node == nullptr or node->control != widget::control_kind::combo_box)
                    it = current.open_combos.erase(it);
                else
                    ++it;
            }

            for (auto it = current.edits.begin(); it != current.edits.end();)
            {
                const auto *node = root.find(it->first);
                if (node == nullptr or node->control != widget::control_kind::text_input)
                {
                    it = current.edits.erase(it);
                    continue;
                }

                auto &edit  = it->second;
                edit.caret  = widget::clamp_offset(node->text, edit.caret);
                edit.anchor = widget::clamp_offset(node->text, edit.anchor);
                ++it;
            }

            if (current.focused)
            {
                const auto *node = root.find(*current.focused);
                if (node == nullptr or !node->focusable or !node->visible)
                    current.focused.reset();
                else if (node->control == widget::control_kind::text_input
                         and !current.edits.contains(node->id))
                    current.edits[node->id].collapse(node->text.size());
            }

            if (current.selecting)
            {
                const auto *node = root.find(*current.selecting);
                if (node == nullptr or node->control != widget::control_kind::text_input)
                    current.selecting.reset();
            }

            if (current.dragging)
            {
                const auto *node = root.find(*current.dragging);
                if (node == nullptr or node->control != widget::control_kind::slider)
                    current.dragging.reset();
            }
        }


        static void set_combo_open(session &current, const widget::node_data &header, bool open)
        {
            if (open)
                current.open_combos.insert(header.id);
            else
                current.open_combos.erase(header.id);

            if (header.parent != nullptr)
                for (auto &sibling : header.parent->children)
                    if (sibling.control == widget::control_kind::combo_option)
                        sibling.visible = open;

            current.dirty = true;
        }


        [[nodiscard]]
        static auto combo_header(const widget::node_data &node) -> const widget::node_data *
        {
            if (node.parent == nullptr) return nullptr;
            for (const auto &sibling : node.parent->children)
                if (sibling.control == widget::control_kind::combo_box) return &sibling;
            return nullptr;
        }


        void choose_combo_option(session &current, const widget::node_data &option)
        {
            emit_control(current, event::kind::selection_changed, option, option.text);

            if (const auto *header = combo_header(option); header != nullptr)
            {
                set_combo_open(current, *header, false);
                focus_node(current, *header);
            }
            else
                current.dirty = true;
        }


        void close_combo(session &current, const widget::node_data &option)
        {
            if (const auto *header = combo_header(option); header != nullptr)
            {
                set_combo_open(current, *header, false);
                focus_node(current, *header);
            }
            else
                current.dirty = true;
        }


        static void close_combos_outside(session &current, const widget::node_data *hit)
        {
            if (current.open_combos.empty()) return;

            for (auto *node = hit; node != nullptr; node = node->parent)
                if (node->control == widget::control_kind::combo_box
                    or node->control == widget::control_kind::combo_option)
                    return;

            current.open_combos.clear();
            if (current.tree) apply_combo_visibility(*current.tree, current.open_combos);
            current.dirty = true;
        }


        [[nodiscard]]
        auto caret_at(render_context          &context,
                      const widget::node_data &node,
                      const math::rect        &bounds,
                      float                    x) const -> std::size_t
        {
            const auto content = bounds.inset(node.padding);
            const auto target  = x - content.position.x;
            if (target <= 0.F) return 0;

            std::size_t best     = 0;
            auto        distance = std::abs(target);
            auto        offset   = std::size_t { 0 };

            while (offset < node.text.size())
            {
                offset = widget::next_offset(node.text, offset);

                auto width = context.measure_text(std::string_view { node.text }.substr(0, offset),
                                                  node.font, std::nullopt);
                if (!width) break;

                const auto candidate = std::abs(width->x - target);
                if (candidate < distance)
                {
                    best     = offset;
                    distance = candidate;
                }
            }

            return best;
        }


        [[nodiscard]]
        auto text_scroll(render_context          &context,
                         const widget::node_data &node,
                         const math::rect        &bounds,
                         std::size_t              caret) const -> float
        {
            const auto content = bounds.inset(node.padding);
            if (content.size.x <= 0.F) return 0.F;

            const auto offset = widget::clamp_offset(node.text, caret);
            auto width = context.measure_text(std::string_view { node.text }.substr(0, offset),
                                              node.font, std::nullopt);
            if (!width) return 0.F;

            return std::max(0.F, width->x - content.size.x + 2.F);
        }


        static void copy_selection(const widget::node_data      &node,
                                   const widget::text_selection &edit)
        {
            if (!edit.has_selection()) return;

            const auto text = widget::selected_text(node.text, edit);
            SDL_SetClipboardText(text.c_str());
        }


        void paste_clipboard(session                &current,
                             widget::node_data      &node,
                             widget::text_selection &edit) const
        {
            auto *clipboard = SDL_GetClipboardText();
            if (clipboard == nullptr) return;

            const auto text = widget::sanitize_single_line(clipboard);
            SDL_free(clipboard);
            if (text.empty()) return;

            widget::insert_text(node.text, edit, text);
            apply_text_edit(current, node);
        }


        void apply_text_edit(session &current, widget::node_data &node) const
        {
            emit_control(current, event::kind::text_edit, node, node.text);
            current.dirty = true;
        }


        auto handle_text_input_key(session           &current,
                                   widget::node_data &node,
                                   const SDL_Event   &native) -> bool
        {
            auto &edit  = current.edits[node.id];
            edit.caret  = widget::clamp_offset(node.text, edit.caret);
            edit.anchor = widget::clamp_offset(node.text, edit.anchor);

            const bool shift = (native.key.mod & SDL_KMOD_SHIFT) != 0;
            const bool ctrl  = (native.key.mod & SDL_KMOD_CTRL) != 0;

            switch (native.key.key)
            {
            case SDLK_BACKSPACE:
                if (widget::erase_backward(node.text, edit)) apply_text_edit(current, node);
                return true;

            case SDLK_DELETE:
                if (widget::erase_forward(node.text, edit)) apply_text_edit(current, node);
                return true;

            case SDLK_LEFT:
                widget::move_caret(node.text, edit, widget::previous_offset(node.text, edit.caret),
                                   shift);
                current.dirty = true;
                return true;

            case SDLK_RIGHT:
                widget::move_caret(node.text, edit, widget::next_offset(node.text, edit.caret),
                                   shift);
                current.dirty = true;
                return true;

            case SDLK_HOME:
                widget::move_caret(node.text, edit, 0, shift);
                current.dirty = true;
                return true;

            case SDLK_END:
                widget::move_caret(node.text, edit, node.text.size(), shift);
                current.dirty = true;
                return true;

            case SDLK_A:
                if (!ctrl) return false;
                widget::select_all(node.text, edit);
                current.dirty = true;
                return true;

            case SDLK_C:
                if (!ctrl) return false;
                copy_selection(node, edit);
                return true;

            case SDLK_X:
                if (!ctrl) return false;
                if (edit.has_selection())
                {
                    copy_selection(node, edit);
                    widget::insert_text(node.text, edit, {});
                    apply_text_edit(current, node);
                }
                return true;

            case SDLK_V:
                if (!ctrl) return false;
                paste_clipboard(current, node, edit);
                return true;

            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                emit_control(current, event::kind::submit, node, node.text);
                return true;

            case SDLK_ESCAPE:
                current.focused.reset();
                current.dirty = true;
                return true;

            default: return false;
            }
        }


        auto handle_slider_key(session &current, widget::node_data &node, const SDL_Event &native)
            -> bool
        {
            constexpr auto step  = 0.05F;
            auto           value = node.control_value;

            switch (native.key.key)
            {
            case SDLK_LEFT:  value -= step; break;
            case SDLK_RIGHT: value += step; break;
            case SDLK_HOME:  value = 0.F; break;
            case SDLK_END:   value = 1.F; break;
            default:         return false;
            }

            value = std::clamp(value, 0.F, 1.F);
            if (value == node.control_value) return true;

            node.control_value = value;
            emit_control(current, event::kind::slider_changed, node, {}, value);
            current.dirty = true;
            return true;
        }


        void focus_combo_option(session &current, const widget::node_data &header, bool forward)
        {
            if (header.parent == nullptr) return;

            std::vector<const widget::node_data *> options;
            for (const auto &sibling : header.parent->children)
                if (sibling.control == widget::control_kind::combo_option and sibling.visible)
                    options.push_back(&sibling);

            if (options.empty()) return;
            focus_node(current, *options[forward ? 0 : options.size() - 1]);
        }


        auto handle_combo_key(session &current, widget::node_data &node, const SDL_Event &native)
            -> bool
        {
            const bool is_open = current.open_combos.contains(node.id);

            switch (native.key.key)
            {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_SPACE:    set_combo_open(current, node, !is_open); return true;

            case SDLK_ESCAPE:
                if (!is_open) return false;
                set_combo_open(current, node, false);
                return true;

            case SDLK_DOWN:
                if (!is_open) return false;
                focus_combo_option(current, node, true);
                return true;

            case SDLK_UP:
                if (!is_open) return false;
                focus_combo_option(current, node, false);
                return true;

            default: return false;
            }
        }


        auto handle_combo_option_key(session           &current,
                                     widget::node_data &node,
                                     const SDL_Event   &native) -> bool
        {
            switch (native.key.key)
            {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_SPACE:    choose_combo_option(current, node); return true;

            case SDLK_ESCAPE: close_combo(current, node); return true;

            default: return false;
            }
        }


        auto handle_control_key(session &current, const SDL_Event &native) -> bool
        {
            auto *node = focused_node(current);
            if (node == nullptr) return false;

            switch (node->control)
            {
            case widget::control_kind::text_input:
                return handle_text_input_key(current, *node, native);
            case widget::control_kind::slider:    return handle_slider_key(current, *node, native);
            case widget::control_kind::combo_box: return handle_combo_key(current, *node, native);
            case widget::control_kind::combo_option:
                return handle_combo_option_key(current, *node, native);
            case widget::control_kind::checkbox_indicator:
            case widget::control_kind::radio_indicator:
            case widget::control_kind::none:               break;
            }

            if (node->action.empty()) return false;

            switch (native.key.key)
            {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_SPACE:    emit_control(current, event::kind::activate, *node); return true;
            default:            return false;
            }
        }


        auto handle_focus_key(session &current, const SDL_Event &native) -> bool
        {
            if (native.key.key != SDLK_TAB) return false;

            focus_next(current, (native.key.mod & SDL_KMOD_SHIFT) != 0);
            return true;
        }


        void update_focus_from_hit(session &current, const widget::node_data *hit)
        {
            const auto *target = _impl::focus_target(hit);
            if (target == nullptr)
            {
                if (current.focused) current.dirty = true;
                current.focused.reset();
                current.selecting.reset();
                return;
            }

            focus_node(current, *target);
        }


        void update_slider(session &current, widget::node_data &node, float x)
        {
            if (!current.layout) return;

            const auto *bounds = current.layout->find(node.id);
            if (bounds == nullptr) return;

            const auto width = std::max(1.F, bounds->size.x);
            const auto value = std::clamp((x - bounds->position.x) / width, 0.F, 1.F);
            if (value == node.control_value) return;

            node.control_value = value;
            emit_control(current, event::kind::slider_changed, node, {}, value);
            current.dirty = true;
        }


        void handle_control_press(session                 &current,
                                  render_context          &context,
                                  const widget::node_data *hit,
                                  float                    x)
        {
            const auto *target = _impl::control_target(hit);
            if (target == nullptr) return;

            auto *node = current.tree ? current.tree->find(target->id) : nullptr;
            if (node == nullptr) return;

            switch (node->control)
            {
            case widget::control_kind::text_input:
                {
                    focus_node(current, *node);

                    auto &edit  = current.edits[node->id];
                    edit.caret  = widget::clamp_offset(node->text, edit.caret);
                    edit.anchor = widget::clamp_offset(node->text, edit.anchor);

                    if (const auto *bounds
                        = current.layout ? current.layout->find(node->id) : nullptr)
                    {
                        const auto scroll = text_scroll(context, *node, *bounds, edit.caret);
                        edit.collapse(caret_at(context, *node, *bounds, x + scroll));
                    }
                    else
                        edit.collapse(node->text.size());

                    current.selecting = node->id;
                    current.dirty     = true;
                    break;
                }

            case widget::control_kind::slider:
                focus_node(current, *node);
                current.dragging = node->id;
                update_slider(current, *node, x);
                break;

            case widget::control_kind::combo_box:
                focus_node(current, *node);
                set_combo_open(current, *node, !current.open_combos.contains(node->id));
                break;

            case widget::control_kind::combo_option: choose_combo_option(current, *node); break;

            case widget::control_kind::checkbox_indicator:
            case widget::control_kind::radio_indicator:
            case widget::control_kind::none:               break;
            }
        }


        void handle_control_motion(session &current, render_context &context, math::vec2 point)
        {
            if (current.selecting)
            {
                auto *node = current.tree ? current.tree->find(*current.selecting) : nullptr;
                if (node != nullptr and current.layout)
                    if (const auto *bounds = current.layout->find(node->id); bounds != nullptr)
                    {
                        auto &edit  = current.edits[node->id];
                        edit.caret  = widget::clamp_offset(node->text, edit.caret);
                        edit.anchor = widget::clamp_offset(node->text, edit.anchor);

                        const auto scroll = text_scroll(context, *node, *bounds, edit.caret);
                        widget::move_caret(node->text, edit,
                                           caret_at(context, *node, *bounds, point.x + scroll),
                                           true);
                        current.dirty = true;
                    }
            }

            if (current.dragging)
            {
                auto *node = current.tree ? current.tree->find(*current.dragging) : nullptr;
                if (node != nullptr) update_slider(current, *node, point.x);
            }
        }


        void handle_pointer_move(session &current, render_context &context, const SDL_Event &native)
        {
            const auto *hit   = hit_at(current, native.motion.x, native.motion.y);
            const auto  point = to_logical(native.motion.x, native.motion.y);

            handle_control_motion(current, context, point);

            auto next_hover = visual_target(hit);
            if (current.hovered != next_hover)
            {
                current.hovered = std::move(next_hover);
                current.dirty   = true;
            }

            auto value = make_event(event::kind::mouse_move, hit);
            value.x    = point.x;
            value.y    = point.y;
            dispatch(value, current);
        }


        void
        handle_pointer_button(session &current, render_context &context, const SDL_Event &native)
        {
            const bool  is_down = native.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
            const auto *hit     = hit_at(current, native.button.x, native.button.y);
            const auto  point   = to_logical(native.button.x, native.button.y);

            if (native.button.button == SDL_BUTTON_LEFT and is_down)
            {
                close_combos_outside(current, hit);
                update_focus_from_hit(current, hit);
                handle_control_press(current, context, hit, point.x);
            }

            auto value = make_event(is_down ? event::kind::mouse_down : event::kind::mouse_up, hit);
            value.x    = point.x;
            value.y    = point.y;
            value.button = native.button.button;

            if (native.button.button == SDL_BUTTON_LEFT and !is_down)
                if (!release_matches_press(current, hit)) value.action.clear();

            dispatch(value, current);

            if (native.button.button == SDL_BUTTON_LEFT)
            {
                if (!is_down)
                {
                    current.selecting.reset();
                    current.dragging.reset();
                }

                update_press_state(current, hit, is_down);
            }
        }


        void handle_wheel(session &current, const SDL_Event &native)
        {
            const auto *hit   = hit_at(current, native.wheel.mouse_x, native.wheel.mouse_y);
            auto        value = make_event(event::kind::scroll, hit);
            const auto  point = to_logical(native.wheel.mouse_x, native.wheel.mouse_y);
            value.x           = point.x;
            value.y           = point.y;
            value.scroll_x    = native.wheel.x;
            value.scroll_y    = native.wheel.y;
            dispatch(value, current);
        }


        void handle_key(session &current, const SDL_Event &native)
        {
            if (native.type == SDL_EVENT_KEY_DOWN)
            {
                if (handle_focus_key(current, native)) return;
                if (handle_control_key(current, native)) return;
            }

            auto value      = make_event(native.type == SDL_EVENT_KEY_DOWN ? event::kind::key_down
                                                                           : event::kind::key_up,
                                         nullptr);
            value.key       = native.key.key;
            value.modifiers = static_cast<std::uint16_t>(native.key.mod);
            value.repeat    = native.key.repeat;
            dispatch(value, current);
        }


        void handle_text_input(session &current, const SDL_Event &native)
        {
            if (native.text.text != nullptr)
            {
                auto *node = focused_node(current);
                if (node != nullptr and node->control == widget::control_kind::text_input)
                {
                    const auto text = widget::sanitize_single_line(native.text.text);
                    if (!text.empty())
                    {
                        auto &edit  = current.edits[node->id];
                        edit.caret  = widget::clamp_offset(node->text, edit.caret);
                        edit.anchor = widget::clamp_offset(node->text, edit.anchor);
                        widget::insert_text(node->text, edit, text);
                        apply_text_edit(current, *node);
                    }
                    return;
                }
            }

            auto value = make_event(event::kind::text_input, nullptr);
            if (native.text.text != nullptr) value.text = native.text.text;
            dispatch(value, current);
        }


        void handle_resize(session &current, render_context &context)
        {
            SDL_GetWindowSize(m_window.get(), &m_width, &m_height);
            SDL_SetRenderLogicalPresentation(m_renderer.get(), m_width, m_height,
                                             SDL_LOGICAL_PRESENTATION_LETTERBOX);
            context.set_viewport({ static_cast<float>(m_width), static_cast<float>(m_height) });

            auto value   = make_event(event::kind::resize, nullptr);
            value.width  = m_width;
            value.height = m_height;
            dispatch(value, current);
            current.dirty = true;
        }


        void handle_event(session &current, render_context &context, const SDL_Event &native)
        {
            switch (native.type)
            {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                dispatch(make_event(event::kind::quit, nullptr), current);
                current.quit_requested = true;
                break;

            case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            case SDL_EVENT_WINDOW_FOCUS_LOST:  clear_pointer_state(current); break;

            case SDL_EVENT_WINDOW_EXPOSED:
            case SDL_EVENT_WINDOW_RESTORED: current.dirty = true; break;

            case SDL_EVENT_WINDOW_RESIZED: handle_resize(current, context); break;

            case SDL_EVENT_MOUSE_MOTION: handle_pointer_move(current, context, native); break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: handle_pointer_button(current, context, native); break;

            case SDL_EVENT_MOUSE_WHEEL: handle_wheel(current, native); break;

            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:   handle_key(current, native); break;

            case SDL_EVENT_TEXT_INPUT: handle_text_input(current, native); break;

            default: break;
            }
        }


        auto pump_messages(session &current) -> bool
        {
            const auto emit
                = [&current](Message message) { current.messages.push_back(std::move(message)); };

            while (!current.messages.empty())
            {
                auto message = std::move(current.messages.front());
                current.messages.pop_front();

                if (auto result = m_update(current.state, message, emit); !result)
                {
                    std::println(stderr, "sleet: update failed: {}", result.error().what());
                    return false;
                }

                current.dirty = true;
            }

            return true;
        }


        void pump_subscription(session &current) const
        {
            if (!m_subscription) return;
            if (auto message = m_subscription())
            {
                current.messages.push_back(std::move(*message));
                current.dirty = true;
            }
        }


        template <typename Measure>
        auto refresh(session &current, render_context &context, const Measure &measure) -> bool
        {
            auto next_tree = m_view(current.state);
            if (!next_tree)
            {
                std::println(stderr, "sleet: view failed: {}", next_tree.error().what());
                return false;
            }

            if (auto result = next_tree->finalize(); !result)
            {
                std::println(stderr, "sleet: widget tree failed: {}", result.error().what());
                return false;
            }

            sync_controls(current, *next_tree);

            auto next_layout = calculate_layout(
                *next_tree, measure, { static_cast<float>(m_width), static_cast<float>(m_height) });
            if (!next_layout)
            {
                std::println(stderr, "sleet: layout failed: {}", next_layout.error().what());
                return false;
            }

            current.tree   = std::move(*next_tree);
            current.layout = std::move(*next_layout);

            context.set_hovered(current.hovered);
            context.set_pressed(current.pressed);
            context.set_focused(current.focused);

            if (auto *node = current.focused ? current.tree->find(*current.focused) : nullptr;
                node != nullptr and node->control == widget::control_kind::text_input)
                context.set_text_selection(node->id, current.edits[node->id]);
            else
                context.set_text_selection(std::nullopt, {});

            if (auto result = context.render(*current.tree, *current.layout); !result)
            {
                std::println(stderr, "sleet: render failed: {}", result.error().what());
                return false;
            }

            if (!SDL_RenderPresent(m_renderer.get()))
            {
                std::println(stderr, "sleet: presenting the frame failed: {}", SDL_GetError());
                return false;
            }

            current.dirty = false;
            return true;
        }


        auto run_impl() -> int
        {
            auto context = render_context { m_renderer.get(), m_theme };
            context.set_viewport({ static_cast<float>(m_width), static_cast<float>(m_height) });

            const auto text_input = _impl::text_input_guard { m_window.get() };

            auto booted = m_boot();
            if (!booted)
            {
                std::println(stderr, "sleet: boot failed: {}", booted.error().what());
                return 1;
            }

            session current { std::move(booted->first) };
            current.messages.push_back(std::move(booted->second));

            const auto measure = [&context](std::string_view text, const widget::font_spec &font,
                                            std::optional<float> width_limit)
            { return context.measure_text(text, font, width_limit); };

            while (true)
            {
                if (!pump_messages(current)) return 1;
                if (current.quit_requested) break;

                pump_subscription(current);

                if (current.dirty and !refresh(current, context, measure)) return 1;

                SDL_Event native;
                if (!SDL_PollEvent(&native))
                    if (!SDL_WaitEventTimeout(&native, 16)) continue;

                do { handle_event(current, context, native); } while (SDL_PollEvent(&native));
            }

            return 0;
        }
    };
}
