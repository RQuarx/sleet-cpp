#pragma once
#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <deque>
#include <exception>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <print>
#include <ranges>
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
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <kei-utils/runtime_guard.hh>

#include "sleet/error.hh"
#include "sleet/render_context.hh"
#include "sleet/shortcut.hh"
#include "sleet/traits.hh"
#include "sleet/widget/control.hh"
#include "sleet/widget/layout.hh"
#include "sleet/widget/node.hh"
#include "sleet/widget/scroll.hh"
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

        /// The selected row keys reported by a multi-select list, table, or
        /// tree. Row keys are their actions, or their text when no action is
        /// set. The keys are ordered by their visible row order.
        std::vector<std::string> selection;

        /// A normalized control value in the range zero to one. Sliders use it.
        float value = 0.F;

        int width  = 0;
        int height = 0;
    };


    namespace _impl
    {
        /// A non-owning id lookup for a widget tree. Keys are views into node
        /// ids, so an index is only valid while its tree stays in place.
        using node_index = std::unordered_map<std::string_view, widget::node_data *>;


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


        /// True when \p node is the popup surface or one of its descendants.
        [[nodiscard]]
        inline auto inside_combo_popup(const widget::node_data *node) noexcept -> bool
        {
            for (auto *current = node; current != nullptr; current = current->parent)
                if (current->control == widget::control_kind::combo_popup) return true;
            return false;
        }


        [[nodiscard]]
        inline auto interactive_target(const widget::node_data *node) noexcept
            -> const widget::node_data *
        {
            for (auto *current = node; current != nullptr; current = current->parent)
            {
                // Indicators are visual pieces of their parent control. Treating
                // them as separate targets would break press/release matching
                // and hide the parent's hover feedback. A combo popup is a
                // surface, not an interactive control of its own.
                if (current->control == widget::control_kind::checkbox_indicator
                    or current->control == widget::control_kind::radio_indicator
                    or current->control == widget::control_kind::combo_popup)
                    continue;

                if (!current->action.empty() or current->control != widget::control_kind::none)
                    return current;
            }
            return nullptr;
        }


        /// True when the node and every ancestor are visible and enabled.
        [[nodiscard]]
        inline auto effectively_visible(const widget::node_data &node) noexcept -> bool
        {
            for (const auto *current = &node; current != nullptr; current = current->parent)
                if (!current->visible or current->disabled) return false;
            return true;
        }


        /// True when \p node is \p ancestor or one of its descendants.
        [[nodiscard]]
        inline auto inside_subtree(const widget::node_data &node,
                                   const widget::node_data &ancestor) noexcept -> bool
        {
            for (const auto *current = &node; current != nullptr; current = current->parent)
                if (current == &ancestor) return true;
            return false;
        }


        /// Orders two modal candidates exactly as the renderer paints them.
        struct modal_paint_key
        {
            bool        floating       = false;
            int         z_index        = 0;
            std::size_t floating_order = 0;
            std::size_t order          = 0;
        };


        [[nodiscard]]
        inline auto paints_above(const modal_paint_key &lhs, const modal_paint_key &rhs) noexcept
            -> bool
        {
            // In-flow content is painted before every floating node.
            if (lhs.floating != rhs.floating) return lhs.floating;

            if (lhs.floating)
            {
                // Floating nodes sort by z-index, then by the document order
                // of their outermost floating ancestor. A nested floating node
                // is its own entry in the global floating pass.
                if (lhs.z_index != rhs.z_index) return lhs.z_index > rhs.z_index;
                if (lhs.floating_order != rhs.floating_order)
                    return lhs.floating_order > rhs.floating_order;
            }

            return lhs.order > rhs.order;
        }


        inline void scan_modals(const widget::node_data  &node,
                                bool                      floating,
                                int                       floating_z,
                                std::size_t               floating_order,
                                std::size_t              &order,
                                const widget::node_data *&best,
                                modal_paint_key          &best_key) noexcept
        {
            if (!effectively_visible(node)) return;

            const auto node_order = order++;

            if (node.floating)
            {
                floating       = true;
                floating_z     = node.z_index;
                floating_order = node_order;
            }

            if (node.modal)
            {
                const auto key = modal_paint_key {
                    .floating       = floating,
                    .z_index        = floating ? floating_z : 0,
                    .floating_order = floating_order,
                    .order          = node_order,
                };

                if (best == nullptr or paints_above(key, best_key))
                {
                    best     = &node;
                    best_key = key;
                }
            }

            for (const auto &child : node.children)
                scan_modals(child, floating, floating_z, floating_order, order, best, best_key);
        }


        /// Returns the nearest node, starting at \p node, that has tooltip
        /// text. Returning null means hovering this subtree should not schedule
        /// a tooltip wakeup.
        [[nodiscard]]
        inline auto tooltip_owner(const widget::node_data *node) noexcept
            -> const widget::node_data *
        {
            for (auto *cursor = node; cursor != nullptr; cursor = cursor->parent)
                if (!cursor->tooltip_text.empty()) return cursor;

            return nullptr;
        }


        /// Returns the topmost visible modal surface, in paint order. A modal
        /// blocks pointer input to everything outside its subtree.
        [[nodiscard]]
        inline auto active_modal(const widget::node_data &root) noexcept
            -> const widget::node_data *
        {
            // Floating nodes paint above in-flow content, sorted by z-index.
            // Selecting the last modal in document order would target a dialog
            // that is visually underneath another.
            const widget::node_data *result   = nullptr;
            auto                     best_key = modal_paint_key {};
            auto                     order    = std::size_t { 0 };

            scan_modals(root, false, 0, 0, order, result, best_key);
            return result;
        }


        /// Returns the node's border box intersected with every clipping
        /// ancestor. The result is empty when the node is fully clipped away,
        /// hidden, or disabled by an ancestor.
        [[nodiscard]]
        inline auto visible_bounds(const widget::node_data     &node,
                                   const widget::layout_result &layout) noexcept
            -> std::optional<math::rect>
        {
            if (!node.visible or node.disabled) return std::nullopt;

            const auto *bounds = layout.find(node.id);
            if (bounds == nullptr) return std::nullopt;

            auto result = *bounds;

            for (auto *current = node.parent; current != nullptr; current = current->parent)
            {
                if (!current->visible or current->disabled) return std::nullopt;
                if (!current->clip_children) continue;

                const auto *clip = layout.find(current->id);
                if (clip == nullptr) return std::nullopt;

                const auto left   = std::max(result.position.x, clip->position.x);
                const auto top    = std::max(result.position.y, clip->position.y);
                const auto right  = std::min(result.right(), clip->right());
                const auto bottom = std::min(result.bottom(), clip->bottom());

                if (right <= left or bottom <= top) return std::nullopt;
                result = { left, top, right - left, bottom - top };
            }

            return result;
        }


        [[nodiscard]]
        inline auto hit_test_subtree(const widget::node_data     &node,
                                     const widget::layout_result &layout,
                                     math::vec2 point) noexcept -> const widget::node_data *
        {
            const auto bounds = visible_bounds(node, layout);
            if (!bounds or !bounds->contains(point)) return nullptr;

            // Floating descendants are resolved by the global overlay pass so
            // they can extend beyond this node's bounds.
            for (auto it = node.children.rbegin(); it != node.children.rend(); ++it)
            {
                if (it->floating) continue;
                if (auto *hit = hit_test_subtree(*it, layout, point); hit != nullptr) return hit;
            }

            return &node;
        }


        [[nodiscard]]
        inline auto hit_test(const widget::node_data     &root,
                             const widget::layout_result &layout,
                             math::vec2 point) noexcept -> const widget::node_data *
        {
            // A modal limits hit testing to its own subtree. Clicks outside
            // the modal surface still target the modal itself so a menu or
            // dialog can dismiss itself through its action or cancel action.
            if (const auto *modal = active_modal(root); modal != nullptr)
            {
                for (auto it = layout.floating.rbegin(); it != layout.floating.rend(); ++it)
                {
                    const auto *node = root.find(it->id);
                    if (node == nullptr or !inside_subtree(*node, *modal)) continue;
                    if (auto *hit = hit_test_subtree(*node, layout, point); hit != nullptr)
                        return hit;
                }

                if (auto *hit = hit_test_subtree(*modal, layout, point); hit != nullptr) return hit;
                return modal;
            }

            // Overlays are painted last, so they are tested first, in reverse
            // paint order.
            for (auto it = layout.floating.rbegin(); it != layout.floating.rend(); ++it)
            {
                const auto *node = root.find(it->id);
                if (node == nullptr) continue;
                if (auto *hit = hit_test_subtree(*node, layout, point); hit != nullptr) return hit;
            }

            return hit_test_subtree(root, layout, point);
        }


        /// Builds the per-frame interaction targets for the renderer. Hover and
        /// press are recorded on the target widget only. The renderer already
        /// inherits progress from a node's ancestors, which lets a button's
        /// contents follow the button; marking ancestors here as well would
        /// leak the hover state from the root into every sibling in the tree.
        [[nodiscard]]
        inline auto interaction_targets(const widget::node_data                &root,
                                        const std::optional<widget::widget_id> &hovered,
                                        const std::optional<widget::widget_id> &pressed,
                                        const std::optional<widget::widget_id> &focused,
                                        bool                                    show_focus,
                                        const node_index                       *index = nullptr)
            -> std::unordered_map<widget::widget_id, interaction_progress>
        {
            auto output = std::unordered_map<widget::widget_id, interaction_progress> {};

            const auto exists = [&](const widget::widget_id &id)
            {
                if (index != nullptr)
                {
                    const auto it = index->find(id.view());
                    return it != index->end() and it->second != nullptr;
                }

                return root.find(id) != nullptr;
            };

            if (hovered and exists(*hovered)) output[*hovered].hover = 1.F;
            if (pressed and exists(*pressed)) output[*pressed].press = 1.F;

            if (show_focus and focused and exists(*focused)) output[*focused].focus = 1.F;

            return output;
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

            m_vsync_active = SDL_SetRenderVSync(m_renderer.get(), 1);

            // Layout is expressed in window points. The renderer draws in
            // physical pixels, so high-DPI windows use a pixel scale instead of
            // SDL's logical presentation (which resamples the whole frame and
            // makes text blurry).
            m_text_cursor    = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
            m_pointer_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
            m_move_cursor    = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE);
        }

        ~application()
        {
            if (m_text_input_active) SDL_StopTextInput(m_window.get());
            if (m_current_cursor != nullptr) SDL_SetCursor(SDL_GetDefaultCursor());
            if (m_text_cursor != nullptr) SDL_DestroyCursor(m_text_cursor);
            if (m_pointer_cursor != nullptr) SDL_DestroyCursor(m_pointer_cursor);
            if (m_move_cursor != nullptr) SDL_DestroyCursor(m_move_cursor);
        }

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

        /// Registers a keyboard shortcut that is active regardless of focus.
        ///
        /// A shortcut dispatches a \c key_down event with its action. Modifiers
        /// must match exactly; Caps Lock and Num Lock are ignored. Key repeats
        /// are ignored unless \c key_chord::repeat is set. While a text input is
        /// focused, shortcuts without Ctrl, Alt, or Super are skipped so typing
        /// is never stolen; function keys still work.
        ///
        /// Shortcuts declared on a visible widget subtree take precedence over
        /// these application-wide bindings.
        /// \param chord The key and exact modifier mask.
        /// \param action The action reported by the resulting \c key_down event.
        auto on_shortcut(key_chord chord, std::string action) -> application &
        {
            if (chord.key == 0)
            {
                if (m_shortcut_error.empty()) m_shortcut_error = "sleet: a shortcut needs a key.";
                return *this;
            }

            chord.modifiers = shortcut_modifiers(chord.modifiers);
            m_shortcuts.push_back({ chord, std::move(action) });
            return *this;
        }

        /// Registers a keyboard shortcut from a key code and modifier mask.
        /// \param key The SDL key code.
        /// \param modifiers The SDL modifier mask.
        /// \param action The action reported by the resulting \c key_down event.
        /// \param repeat Whether the shortcut also fires while the key repeats.
        auto on_shortcut(std::uint32_t key,
                         std::uint16_t modifiers,
                         std::string   action,
                         bool          repeat = false) -> application &
        { return on_shortcut(key_chord { key, modifiers, repeat }, std::move(action)); }

        /// Registers a keyboard shortcut from text such as \c "Ctrl+Shift+S",
        /// \c "Cmd+Enter", or \c "F5".
        ///
        /// An invalid shortcut is reported by \c run(), which returns a
        /// non-zero exit code before starting the event loop.
        /// \param chord The shortcut text.
        /// \param action The action reported by the resulting \c key_down event.
        auto on_shortcut(std::string_view chord, std::string action) -> application &
        {
            auto parsed = parse_key_chord(chord);
            if (!parsed)
            {
                if (m_shortcut_error.empty())
                    m_shortcut_error = std::format("sleet: invalid shortcut \"{}\".", chord);
                return *this;
            }

            return on_shortcut(*parsed, std::move(action));
        }

        /// Replaces the theme. Call from an update callback or the event-loop
        /// thread; the application is not thread-safe.
        void set_theme(widget::theme theme)
        {
            // Apply immediately when the loop is running; otherwise the new
            // theme is picked up when the render context is created. Keep the
            // previous theme if the live context rejects the new fonts.
            if (m_context != nullptr)
            {
                m_context->set_theme(theme);
                m_theme            = std::move(theme);
                m_redraw_requested = true;
                return;
            }

            m_theme            = std::move(theme);
            m_redraw_requested = true;
        }

        /// Turns easing off when \p value is true. Reduced motion snaps every
        /// transition to its target, which is useful for accessibility and for
        /// deterministic screenshots.
        ///
        /// Call from an update callback or the event-loop thread; the
        /// application is not thread-safe.
        void set_reduced_motion(bool value = true) noexcept { m_reduced_motion = value; }


        /// Sets how long the pointer must rest before a tooltip appears.
        /// A delay of zero shows tooltips immediately.
        ///
        /// Call from an update callback or the event-loop thread; the
        /// application is not thread-safe.
        void set_tooltip_delay(std::uint32_t milliseconds) noexcept
        { m_tooltip_delay_ms = milliseconds; }


        /// Asks the event loop to stop after the current iteration. This is
        /// equivalent to closing the window, but it also works from a
        /// subscription or update callback.
        void request_quit() noexcept { m_quit_requested = true; }


        /// Shows a transient notification in the bottom-right corner. Toasts
        /// fade in, stack upward, and disappear after \p seconds.
        ///
        /// Call this from an update callback (capture the application) or from
        /// the thread that runs the event loop. It is not thread-safe.
        void notify(std::string text, float seconds = 3.F)
        {
            if (text.empty()) return;

            constexpr auto max_visible = std::size_t { 4 };
            if (m_toasts.size() >= max_visible) m_toasts.pop_front();

            const auto duration = std::isfinite(seconds) ? std::max(0.5F, seconds) : 3.F;
            m_toasts.push_back({ std::move(text), 0.F, duration });
            m_redraw_requested = true;
        }

        /// Removes every active toast immediately. Call from the event-loop
        /// thread, like \c notify.
        void clear_toasts()
        {
            if (m_toasts.empty()) return;
            m_toasts.clear();
            m_redraw_requested = true;
        }


        /// Returns the selected row keys of a multi-select container, ordered
        /// by their visible row order. Row keys are their actions, or their
        /// text when no action is set.
        [[nodiscard]]
        auto selected_rows(const widget::widget_id &container) const -> std::vector<std::string>
        {
            const auto it = m_selections.find(container);
            return it == m_selections.end() ? std::vector<std::string> {} : it->second.rows;
        }


        /// Replaces the selection of a multi-select container. The runtime
        /// paints matching rows on the next frame. Unknown keys are kept but
        /// have no effect until a matching row appears.
        void set_selected_rows(const widget::widget_id &container, std::vector<std::string> rows)
        {
            auto &state       = m_selections[container];
            state.initialized = true;
            state.rows        = std::move(rows);
            state.anchor = state.rows.empty() ? std::nullopt
                                              : std::optional<std::string> { state.rows.front() };
            m_selection_requested = true;
        }


        /// Clears the runtime selection of one multi-select container.
        void clear_selection(const widget::widget_id &container)
        {
            auto &state       = m_selections[container];
            state.initialized = true;
            state.rows.clear();
            state.anchor.reset();
            m_selection_requested = true;
        }


        /// Clears every runtime-managed multi-selection.
        void clear_selections()
        {
            for (auto &[container, state] : m_selections)
            {
                (void)container;
                state.initialized = true;
                state.rows.clear();
                state.anchor.reset();
            }

            m_selection_requested = true;
        }


        auto run() -> int
        {
            if (!m_shortcut_error.empty())
            {
                std::println(stderr, "{}", m_shortcut_error);
                return 1;
            }

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

        SDL_Cursor *m_text_cursor    = nullptr;
        SDL_Cursor *m_pointer_cursor = nullptr;
        SDL_Cursor *m_move_cursor    = nullptr;
        SDL_Cursor *m_current_cursor = nullptr;

        /// Whether presentation blocks on the display refresh. Without it the
        /// loop paces animated frames itself.
        bool m_vsync_active = false;

        render_context *m_context = nullptr;

        /// Set by set_theme and notify so the loop repaints even when no
        /// input arrives. The application is single-threaded: callbacks and
        /// setters run on the event-loop thread.
        bool m_redraw_requested = false;

        /// Disables easing globally, for accessibility settings.
        bool m_reduced_motion = false;

        /// How long the pointer must rest before a tooltip appears.
        std::uint32_t m_tooltip_delay_ms = 550;

        /// Application-wide keyboard shortcuts, checked after widget-scoped
        /// bindings. The first matching binding wins.
        std::vector<shortcut_binding> m_shortcuts;

        /// The first invalid shortcut definition, reported by run().
        std::string m_shortcut_error;

        /// Set by request_quit so callbacks can stop the loop.
        bool m_quit_requested = false;

        /// Whether SDL text input is currently enabled for the window.
        bool m_text_input_active = false;

        /// Whether an IME candidate area is currently registered.
        bool m_text_input_area_set = false;

        /// Last area registered with the platform IME. Skipping identical
        /// updates keeps candidate windows from being repositioned every frame.
        SDL_Rect m_text_input_area {};
        int      m_text_input_cursor = 0;

        struct toast_entry
        {
            std::string text;
            float       age      = 0.F;
            float       duration = 3.F;
        };

        static constexpr auto toast_fade_in  = 0.16F;
        static constexpr auto toast_fade_out = 0.30F;

        std::deque<toast_entry> m_toasts;

        /// Runtime-owned selection for multi-select containers. Keys are row
        /// actions, or row text when a row has no action.
        struct multi_selection_state
        {
            bool                       initialized = false;
            std::optional<std::string> anchor;
            std::vector<std::string>   rows;
        };

        std::unordered_map<widget::widget_id, multi_selection_state> m_selections;

        /// Set when a public selection setter changes state outside the view
        /// rebuild path.
        bool m_selection_requested = false;

        int           m_width  = 800;
        int           m_height = 600;
        widget::theme m_theme;

        boot_fn         m_boot;
        update_fn       m_update;
        view_fn         m_view;
        event_fn        m_event;
        subscription_fn m_subscription;


        /// Maps ids to nodes without copying them. The views point into the
        /// tree, so the index is rebuilt whenever the tree is replaced.
        using node_index = _impl::node_index;


        struct session
        {
            struct edit_snapshot
            {
                std::string            text;
                widget::text_selection selection;
            };

            explicit session(State initial) : state { std::move(initial) } {}

            State                                state;
            std::deque<Message>                  messages;
            std::optional<widget::node_data>     tree;
            std::optional<widget::layout_result> layout;
            node_index                           nodes;
            std::optional<widget::widget_id>     hovered;
            std::optional<widget::widget_id>     pointer_target;
            std::optional<widget::widget_id>     pressed;
            std::optional<widget::widget_id>     focused;
            std::optional<widget::widget_id>     selecting;
            std::optional<widget::widget_id>     dragging;

            /// An in-progress scrollbar thumb drag.
            struct scrollbar_drag_state
            {
                widget::widget_id scroller;
                bool              horizontal = false;
                float             grab       = 0.F;
            };

            std::optional<scrollbar_drag_state> scrollbar_drag;

            /// An in-progress middle-button pan.
            struct pan_state
            {
                widget::widget_id scroller;
                math::vec2        origin;
                math::vec2        offset;
            };

            std::optional<pan_state> panning;

            /// The last focused member of each focus group, keyed by the group
            /// node's id. Tab returns to that member when it re-enters.
            std::unordered_map<widget::widget_id, widget::widget_id> focus_groups;

            /// Whether any node in the current tree declares a shortcut. When
            /// false, key handling skips the view-scoped search entirely.
            bool has_view_shortcuts = false;

            /// The topmost modal for the current tree, if any. Events use this
            /// to trap focus and route Escape to the cancel action.
            std::optional<widget::widget_id> active_modal;

            /// Focus saved when a modal captures it, restored when it closes.
            std::optional<widget::widget_id> modal_return_focus;
            bool                             modal_focus = false;

            /// Tooltip rest timer and the anchor captured when it appears.
            std::optional<widget::widget_id> tooltip_hover;
            std::uint64_t                    tooltip_started = 0;
            std::string                      tooltip_text;
            math::rect                       tooltip_anchor;
            bool                             tooltip_visible = false;

            std::unordered_map<widget::widget_id, widget::text_selection> edits;

            /// Vertical scroll of each multiline field, in points.
            std::unordered_map<widget::widget_id, float> text_scrolls;

            /// Preferred horizontal caret position while Up and Down move
            /// between visual lines of a multiline field.
            std::unordered_map<widget::widget_id, float> text_caret_goals;

            std::unordered_set<widget::widget_id> open_combos;

            /// Wheel offsets, clamped to the measured content after layout.
            std::unordered_map<widget::widget_id, math::vec2> scrolls;

            /// Desired wheel offsets. The visible offset eases toward these.
            std::unordered_map<widget::widget_id, math::vec2> scroll_targets;

            /// Scrollbar state for the most recent layout.
            std::unordered_map<widget::widget_id, widget::scroll_metrics> scroll_metrics;

            std::unordered_map<widget::widget_id, std::vector<edit_snapshot>> undo;
            std::unordered_map<widget::widget_id, std::vector<edit_snapshot>> redo;

            std::optional<widget::widget_id> preedit_id;
            std::string                      preedit_text;
            std::size_t                      preedit_caret = 0;

            /// Eased hover, press, and focus progress keyed by widget id.
            struct interaction_animation
            {
                widget::animated_value  hover;
                widget::animated_value  press;
                widget::animated_value  focus;
                widget::transition_spec spec;
                bool                    initialized = false;
            };

            std::unordered_map<widget::widget_id, interaction_animation> interactions;

            bool press_active   = false;
            bool focus_visible  = false;
            bool reveal_focus   = false;
            bool quit_requested = false;

            /// Requests any repaint, including render-only changes.
            bool dirty = true;

            /// The widget tree must be rebuilt from the view function.
            bool view_dirty = true;

            /// The existing tree must be measured again, without rebuilding it.
            bool layout_dirty = true;

            /// Smooth scrolling still has to advance on the next frame.
            bool scroll_animating = false;
        };


        static void mark_view_dirty(session &current) noexcept
        {
            current.dirty        = true;
            current.view_dirty   = true;
            current.layout_dirty = true;
        }


        static void mark_layout_dirty(session &current) noexcept
        {
            current.dirty        = true;
            current.layout_dirty = true;
        }


        static void index_nodes(widget::node_data &node, node_index &out, bool &has_shortcuts)
        {
            out.emplace(node.id.view(), &node);
            has_shortcuts = has_shortcuts or !node.shortcuts.empty();
            for (auto &child : node.children) index_nodes(child, out, has_shortcuts);
        }


        /// Rebuilds the id lookup. The tree must already live in its final
        /// location because the index stores views into its ids.
        static void rebuild_node_index(session &current)
        {
            current.nodes.clear();
            current.has_view_shortcuts = false;
            if (!current.tree) return;

            current.nodes.reserve(current.tree->children_amount() + 1);
            index_nodes(*current.tree, current.nodes, current.has_view_shortcuts);
        }


        [[nodiscard]]
        static auto find_node(session &current, const widget::widget_id &id) -> widget::node_data *
        {
            if (!current.tree) return nullptr;

            if (const auto it = current.nodes.find(id.view()); it != current.nodes.end())
                return it->second;

            // A non-empty index covers the whole tree, so a miss means the id
            // is gone. Only walk when the tree was never indexed.
            if (!current.nodes.empty()) return nullptr;

            return current.tree->find(id);
        }


        /// Physical output pixels per logical window point.
        [[nodiscard]]
        auto pixel_scale() const noexcept -> float
        {
            int output_width  = 0;
            int output_height = 0;
            if (!SDL_GetRenderOutputSize(m_renderer.get(), &output_width, &output_height))
                return 1.F;
            if (output_width <= 0 or m_width <= 0) return 1.F;

            const auto scale = static_cast<float>(output_width) / static_cast<float>(m_width);
            return std::isfinite(scale) and scale > 0.F ? scale : 1.F;
        }


        /// SDL pointer events are already reported in window points, which is
        /// the same unit the layout uses.
        [[nodiscard]]
        static auto to_logical(float window_x, float window_y) noexcept -> math::vec2
        { return { window_x, window_y }; }


        void sync_metrics(session &current, render_context &context)
        {
            SDL_GetWindowSize(m_window.get(), &m_width, &m_height);
            context.set_pixel_scale(pixel_scale());
            context.set_viewport({ static_cast<float>(m_width), static_cast<float>(m_height) });
            mark_view_dirty(current);
        }


        /// Enables IME and text-input events only while an editable field owns
        /// focus. Keeping text input enabled globally would show an on-screen
        /// keyboard and IME candidates even when no field is active.
        void sync_text_input(session &current)
        {
            const auto *focused = current.focused ? find_node(current, *current.focused) : nullptr;

            const auto should_enable = focused != nullptr
                                   and focused->control == widget::control_kind::text_input
                                   and _impl::effectively_visible(*focused);

            if (should_enable == m_text_input_active) return;

            // Starting or stopping text input clears the platform's candidate
            // area, so the next present must register it again.
            m_text_input_area_set = false;

            if (should_enable)
                m_text_input_active = SDL_StartTextInput(m_window.get());
            else
            {
                SDL_StopTextInput(m_window.get());
                m_text_input_active = false;
            }
        }


        /// Tells the platform IME where the focused field and caret are. Native
        /// candidate windows use this to avoid covering the text being edited.
        void sync_text_input_area(session &current, render_context &context)
        {
            const auto *focused = current.focused ? find_node(current, *current.focused) : nullptr;

            const auto editable = m_text_input_active and focused != nullptr
                              and focused->control == widget::control_kind::text_input
                              and current.layout;

            if (!editable)
            {
                if (m_text_input_area_set)
                {
                    SDL_SetTextInputArea(m_window.get(), nullptr, 0);
                    m_text_input_area_set = false;
                }
                return;
            }

            const auto bounds = current.layout->rects.find(focused->id);
            if (bounds == current.layout->rects.end()) return;

            const auto caret = context.text_input_caret_rect(*focused, bounds->second,
                                                             current.edits[focused->id]);

            const SDL_Rect area {
                .x = static_cast<int>(std::floor(bounds->second.position.x)),
                .y = static_cast<int>(std::floor(bounds->second.position.y)),
                .w = std::max(0, static_cast<int>(std::ceil(bounds->second.size.x))),
                .h = std::max(0, static_cast<int>(std::ceil(bounds->second.size.y))),
            };

            const auto cursor = caret
                                  ? std::clamp(static_cast<int>(std::lround(
                                                   caret->position.x - static_cast<float>(area.x))),
                                               0, area.w)
                                  : 0;

            if (m_text_input_area_set and area.x == m_text_input_area.x
                and area.y == m_text_input_area.y and area.w == m_text_input_area.w
                and area.h == m_text_input_area.h and cursor == m_text_input_cursor)
                return;

            if (SDL_SetTextInputArea(m_window.get(), &area, cursor))
            {
                m_text_input_area     = area;
                m_text_input_cursor   = cursor;
                m_text_input_area_set = true;
            }
        }


        struct scrollbar_hit
        {
            const widget::node_data *scroller   = nullptr;
            bool                     horizontal = false;
            bool                     on_thumb   = false;
        };


        void update_cursor(const session &current, const widget::node_data *hit, math::vec2 point)
        {
            auto *next = static_cast<SDL_Cursor *>(nullptr);

            if (current.panning or current.scrollbar_drag)
                next = m_move_cursor;
            else if (scrollbar_at(current, hit, point))
                next = m_move_cursor;
            else if (const auto *control = _impl::control_target(hit);
                     control != nullptr and control->control == widget::control_kind::text_input)
                next = m_text_cursor;
            else if (visual_control_target(hit) != nullptr)
                next = m_pointer_cursor;

            if (next == m_current_cursor) return;
            m_current_cursor = next;
            SDL_SetCursor(next == nullptr ? SDL_GetDefaultCursor() : next);
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
            if (!current.focused) return nullptr;
            return find_node(current, *current.focused);
        }


        // Theme hover and press colors belong to interactive widgets. Plain
        // widgets still report a hit target, but hovering them should not
        // change their color. List and tree surfaces are containers rather
        // than controls: highlighting them while the pointer crosses the gaps
        // between rows makes the whole panel flicker.
        [[nodiscard]]
        static auto visual_control_target(const widget::node_data *hit) -> const widget::node_data *
        {
            const auto *target = _impl::interactive_target(hit);
            if (target == nullptr) return nullptr;
            if (target->control == widget::control_kind::list_view
                or target->control == widget::control_kind::tree_view)
                return nullptr;
            return target;
        }


        [[nodiscard]]
        static auto visual_target(const widget::node_data *hit) -> std::optional<widget::widget_id>
        {
            if (const auto *target = visual_control_target(hit); target != nullptr)
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


        void emit_action(session                 &current,
                         event::kind              type,
                         const widget::node_data &node,
                         std::string              action,
                         std::string              text  = {},
                         float                    value = 0.F) const
        {
            event result;
            result.type   = type;
            result.target = node.id;
            result.action = std::move(action);
            result.text   = std::move(text);
            result.value  = value;
            dispatch(result, current);
        }


        void emit_control(session                 &current,
                          event::kind              type,
                          const widget::node_data &node,
                          std::string              text  = {},
                          float                    value = 0.F) const
        { emit_action(current, type, node, node.action, std::move(text), value); }


        static void clear_pointer_state(session &current)
        {
            if (!current.hovered and !current.pressed and !current.press_active
                and !current.selecting and !current.dragging and !current.pointer_target
                and !current.scrollbar_drag and !current.panning)
                return;

            current.hovered.reset();
            current.pointer_target.reset();
            current.pressed.reset();
            current.selecting.reset();
            current.dragging.reset();
            current.scrollbar_drag.reset();
            current.panning.reset();
            current.press_active = false;
            current.tooltip_hover.reset();
            hide_tooltip(current);
            current.dirty = true;
        }


        static void hide_tooltip(session &current)
        {
            if (!current.tooltip_visible)
            {
                current.tooltip_text.clear();
                return;
            }

            current.tooltip_visible = false;
            current.tooltip_text.clear();
            current.dirty = true;
        }


        /// Restarts the rest timer after pointer movement. Moving within the
        /// same widget still counts as movement, so a tooltip only appears
        /// once the pointer settles.
        static void restart_tooltip(session &current)
        {
            hide_tooltip(current);
            current.tooltip_hover   = current.pointer_target;
            current.tooltip_started = SDL_GetTicks();
        }


        void update_tooltip(session &current) const
        {
            if (!current.tree or !current.layout) return;

            if (!current.pointer_target)
            {
                current.tooltip_hover.reset();
                hide_tooltip(current);
                return;
            }

            if (current.tooltip_hover != current.pointer_target)
            {
                current.tooltip_hover   = current.pointer_target;
                current.tooltip_started = SDL_GetTicks();
                hide_tooltip(current);
                return;
            }

            const auto *node = find_node(current, *current.pointer_target);
            if (node == nullptr)
            {
                // The hovered widget disappeared when the view was rebuilt.
                current.tooltip_hover.reset();
                hide_tooltip(current);
                return;
            }

            const auto *owner  = _impl::tooltip_owner(node);
            const auto *bounds = owner != nullptr ? current.layout->find(owner->id) : nullptr;
            if (bounds == nullptr)
            {
                hide_tooltip(current);
                return;
            }

            if (current.tooltip_visible)
            {
                // Keep the bubble attached when layout moves its owner.
                if (current.tooltip_text != owner->tooltip_text
                    or current.tooltip_anchor != *bounds)
                {
                    current.tooltip_text   = owner->tooltip_text;
                    current.tooltip_anchor = *bounds;
                    current.dirty          = true;
                }
                return;
            }

            if (SDL_GetTicks() - current.tooltip_started < m_tooltip_delay_ms) return;

            current.tooltip_text    = owner->tooltip_text;
            current.tooltip_anchor  = *bounds;
            current.tooltip_visible = true;
            current.dirty           = true;
        }


        /// Returns the number of milliseconds the idle loop may sleep before a
        /// timer needs attention, or -1 to block until an SDL event arrives.
        [[nodiscard]]
        auto idle_timeout(session &current) const -> Sint32
        {
            auto timeout = Sint32 { -1 };

            // Subscriptions are polled, so they keep the historical cadence.
            if (m_subscription) timeout = 16;

            // Only schedule a wakeup when there is actually a tooltip to show.
            // Returning zero for a hovered node without tooltip text made the
            // idle loop spin at full speed until the pointer moved away.
            const auto *node
                = current.pointer_target ? find_node(current, *current.pointer_target) : nullptr;
            const auto *owner = _impl::tooltip_owner(node);
            const auto *bounds
                = owner != nullptr and current.layout ? current.layout->find(owner->id) : nullptr;

            if (bounds != nullptr and !current.tooltip_visible)
            {
                const auto elapsed   = SDL_GetTicks() - current.tooltip_started;
                const auto remaining = elapsed >= m_tooltip_delay_ms
                                         ? Sint32 { 0 }
                                         : static_cast<Sint32>(m_tooltip_delay_ms - elapsed);
                timeout              = timeout < 0 ? remaining : std::min(timeout, remaining);
            }

            // Toasts animate on their own timeline. Wake for fade frames, then
            // sleep until the fade-out starts or the toast expires.
            for (const auto &toast : m_toasts)
            {
                const auto remaining = toast.duration - toast.age;
                auto       next      = Sint32 { 16 };

                if (remaining > toast_fade_out and toast.age >= toast_fade_in)
                    next = static_cast<Sint32>(std::ceil((remaining - toast_fade_out) * 1000.F));

                timeout = timeout < 0 ? next : std::min(timeout, next);
            }

            return timeout;
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


        using focus_group_map = std::unordered_map<widget::widget_id, widget::widget_id>;


        /// Adds one representative for each nested focus group and every
        /// ordinary focusable descendant.
        static void gather_group_members(const widget::node_data                &node,
                                         const focus_group_map                  &groups,
                                         std::vector<const widget::node_data *> &result)
        {
            for (const auto &child : node.children)
            {
                if (!child.visible or child.disabled) continue;

                if (child.focus_group)
                {
                    if (const auto *member = group_representative(child, groups))
                        result.push_back(member);
                    continue;
                }

                if (child.focusable) result.push_back(&child);
                gather_group_members(child, groups, result);
            }
        }


        /// Returns the remembered member of \p group, or its first focusable
        /// descendant when the remembered member is gone.
        [[nodiscard]]
        static auto group_representative(const widget::node_data &group,
                                         const focus_group_map &groups) -> const widget::node_data *
        {
            auto members = std::vector<const widget::node_data *> {};
            gather_group_members(group, groups, members);
            if (members.empty()) return group.focusable ? &group : nullptr;

            if (const auto it = groups.find(group.id); it != groups.end())
                for (const auto *member : members)
                    if (member->id == it->second) return member;

            return members.front();
        }


        static void collect_focusable(const widget::node_data                &node,
                                      const focus_group_map                  &groups,
                                      std::vector<const widget::node_data *> &result)
        {
            if (!node.visible or node.disabled) return;

            if (node.focus_group)
            {
                if (const auto *member = group_representative(node, groups))
                    result.push_back(member);
                return;
            }

            if (node.focusable) result.push_back(&node);
            for (const auto &child : node.children) collect_focusable(child, groups, result);
        }


        /// Finds the widget an autofocus request should land on. A focusable
        /// node focuses itself; a container focuses its first focusable
        /// descendant or its focus group's remembered member.
        [[nodiscard]]
        static auto autofocus_target(const widget::node_data &node, const focus_group_map &groups)
            -> const widget::node_data *
        {
            if (!node.visible or node.disabled) return nullptr;
            if (node.focus_group)
                if (const auto *member = group_representative(node, groups)) return member;
            if (node.focusable) return &node;

            for (const auto &child : node.children)
                if (const auto *target = autofocus_target(child, groups)) return target;

            return nullptr;
        }


        static void find_autofocus(const widget::node_data  &node,
                                   const focus_group_map    &groups,
                                   const widget::node_data *&result)
        {
            if (result != nullptr or !node.visible or node.disabled) return;

            if (node.autofocus)
                if (const auto *target = autofocus_target(node, groups))
                {
                    result = target;
                    return;
                }

            for (const auto &child : node.children) find_autofocus(child, groups, result);
        }


        /// Moves focus to the first autofocus request in the active scope when
        /// no valid widget currently owns it. A widget that is already focused
        /// is never replaced, so autofocus does not steal focus while a user
        /// types or navigates.
        void apply_autofocus(session &current, widget::node_data &root)
        {
            if (current.focused)
                if (const auto *focused = find_node(current, *current.focused);
                    focused != nullptr and focused->focusable
                    and _impl::effectively_visible(*focused))
                    return;

            const widget::node_data *scope = &root;
            if (current.active_modal)
            {
                const auto *modal = find_node(current, *current.active_modal);
                if (modal == nullptr) return;
                scope = modal;
            }

            const widget::node_data *target = nullptr;
            find_autofocus(*scope, current.focus_groups, target);
            if (target != nullptr) focus_node(current, *target);
        }


        void focus_node(session &current, const widget::node_data &node, bool keyboard = false)
        {
            const bool same = current.focused and *current.focused == node.id;

            if (!same)
            {
                current.focused = node.id;
                current.selecting.reset();
                if (node.control == widget::control_kind::text_input)
                    current.edits[node.id].collapse(node.text.size());
                current.preedit_id.reset();
                current.preedit_text.clear();
                current.preedit_caret = 0;
                mark_layout_dirty(current);
            }

            // Remember the active member of every enclosing focus group so Tab
            // returns to it after focus leaves the group.
            for (auto *ancestor = node.parent; ancestor != nullptr; ancestor = ancestor->parent)
                if (ancestor->focus_group) current.focus_groups[ancestor->id] = node.id;

            current.focus_visible = keyboard;
            current.reveal_focus  = true;
        }


        void focus_next(session &current, bool backward)
        {
            if (!current.tree) return;

            // Tab stays inside the innermost focus scope, or inside the active
            // modal when one is open.
            const auto *scope = &*current.tree;

            if (current.active_modal)
            {
                if (const auto *modal = current.tree->find(*current.active_modal)) scope = modal;
            }
            else if (current.focused)
            {
                if (const auto *focused = current.tree->find(*current.focused))
                    for (const auto *cursor = focused; cursor != nullptr; cursor = cursor->parent)
                        if (cursor->focus_scope)
                        {
                            scope = cursor;
                            break;
                        }
            }

            std::vector<const widget::node_data *> nodes;
            collect_focusable(*scope, current.focus_groups, nodes);

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

            focus_node(current, *nodes[index], true);
        }


        /// Moves focus into a newly opened modal, keeps it there while the
        /// modal exists, and restores the previous focus when it closes.
        void enforce_modal_focus(session &current, widget::node_data &root)
        {
            const auto *modal = _impl::active_modal(root);

            if (modal == nullptr)
            {
                current.active_modal.reset();
                if (!current.modal_focus) return;

                current.modal_focus = false;

                if (current.modal_return_focus)
                {
                    auto *previous = find_node(current, *current.modal_return_focus);
                    if (previous != nullptr and previous->focusable
                        and _impl::effectively_visible(*previous))
                        focus_node(current, *previous);
                    else
                        current.focused.reset();

                    current.modal_return_focus.reset();
                }
                else
                {
                    current.focused.reset();
                }

                current.selecting.reset();
                current.dirty = true;
                return;
            }

            current.active_modal = modal->id;

            if (!current.modal_focus)
            {
                current.modal_return_focus = current.focused;
                current.modal_focus        = true;
            }

            if (current.focused)
                if (const auto *focused = find_node(current, *current.focused);
                    focused != nullptr and _impl::inside_subtree(*focused, *modal))
                    return;

            std::vector<const widget::node_data *> nodes;
            collect_focusable(*modal, current.focus_groups, nodes);

            const widget::node_data *target = nullptr;
            find_autofocus(*modal, current.focus_groups, target);
            if (target == nullptr and !nodes.empty()) target = nodes.front();

            if (target == nullptr)
            {
                current.focused.reset();
                current.selecting.reset();
            }
            else
                focus_node(current, *target);

            current.dirty = true;
        }


        static void apply_combo_visibility(widget::node_data                           &node,
                                           const std::unordered_set<widget::widget_id> &open)
        {
            // A combo column owns its header and one floating popup. Hide stale
            // popups even when the header disappeared or changed type between
            // frames. Option rows live inside the popup, so hiding the popup
            // hides the whole subtree for painting, focus, and hit testing.
            auto has_open_header = false;
            for (const auto &child : node.children)
                if (child.control == widget::control_kind::combo_box and open.contains(child.id))
                    has_open_header = true;

            for (auto &child : node.children)
                if (child.control == widget::control_kind::combo_popup)
                    child.visible = has_open_header;

            for (auto &child : node.children) apply_combo_visibility(child, open);
        }


        void sync_controls(session &current, widget::node_data &root)
        {
            for (auto it = current.open_combos.begin(); it != current.open_combos.end();)
            {
                const auto *node = find_node(current, *it);
                if (node == nullptr or node->control != widget::control_kind::combo_box
                    or !_impl::effectively_visible(*node))
                    it = current.open_combos.erase(it);
                else
                    ++it;
            }

            apply_combo_visibility(root, current.open_combos);

            for (auto it = current.edits.begin(); it != current.edits.end();)
            {
                const auto *node = find_node(current, it->first);
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

            const auto is_text_input = [&current](const widget::widget_id &id)
            {
                const auto *node = find_node(current, id);
                return node != nullptr and node->control == widget::control_kind::text_input;
            };

            for (auto it = current.undo.begin(); it != current.undo.end();)
                if (!is_text_input(it->first))
                    it = current.undo.erase(it);
                else
                    ++it;

            for (auto it = current.redo.begin(); it != current.redo.end();)
                if (!is_text_input(it->first))
                    it = current.redo.erase(it);
                else
                    ++it;

            for (auto it = current.scrolls.begin(); it != current.scrolls.end();)
            {
                const auto *node = find_node(current, it->first);
                if (node == nullptr or !node->scrollable)
                    it = current.scrolls.erase(it);
                else
                    ++it;
            }

            for (auto it = current.scroll_targets.begin(); it != current.scroll_targets.end();)
            {
                const auto *node = find_node(current, it->first);
                if (node == nullptr or !node->scrollable)
                    it = current.scroll_targets.erase(it);
                else
                    ++it;
            }

            if (current.focused)
            {
                const auto *node = find_node(current, *current.focused);
                if (node == nullptr or !node->focusable or !_impl::effectively_visible(*node))
                    current.focused.reset();
                else if (node->control == widget::control_kind::text_input
                         and !current.edits.contains(node->id))
                    current.edits[node->id].collapse(node->text.size());
            }

            for (auto it = current.focus_groups.begin(); it != current.focus_groups.end();)
            {
                const auto *group  = find_node(current, it->first);
                const auto *member = find_node(current, it->second);

                if (group == nullptr or !group->focus_group or member == nullptr
                    or !member->focusable or !_impl::effectively_visible(*member)
                    or !_impl::inside_subtree(*member, *group))
                    it = current.focus_groups.erase(it);
                else
                    ++it;
            }

            if (current.selecting)
            {
                const auto *node = find_node(current, *current.selecting);
                if (node == nullptr or node->control != widget::control_kind::text_input
                    or !_impl::effectively_visible(*node))
                    current.selecting.reset();
            }

            if (current.hovered)
            {
                const auto *node = find_node(current, *current.hovered);
                if (node == nullptr or !_impl::effectively_visible(*node)) current.hovered.reset();
            }

            if (current.pressed)
            {
                const auto *node = find_node(current, *current.pressed);
                if (node == nullptr or !_impl::effectively_visible(*node))
                {
                    current.pressed.reset();
                    current.press_active = false;
                }
            }

            if (current.dragging)
            {
                const auto *node = find_node(current, *current.dragging);
                if (node == nullptr or node->control != widget::control_kind::slider
                    or !_impl::effectively_visible(*node))
                    current.dragging.reset();
            }

            if (current.preedit_id)
            {
                const auto *node = find_node(current, *current.preedit_id);
                if (node == nullptr or node->control != widget::control_kind::text_input
                    or !_impl::effectively_visible(*node) or !current.focused
                    or *current.focused != *current.preedit_id)
                {
                    current.preedit_id.reset();
                    current.preedit_text.clear();
                    current.preedit_caret = 0;
                }
            }
        }


        static void set_combo_open(session &current, const widget::node_data &header, bool open)
        {
            // Only one popup may be open at a time. Opening a new combo closes
            // the previous one before its options are hidden.
            current.open_combos.clear();
            if (open) current.open_combos.insert(header.id);

            if (current.tree.has_value())
                apply_combo_visibility(*current.tree, current.open_combos);
            else if (header.parent != nullptr)
                for (auto &sibling : header.parent->children)
                    if (sibling.control == widget::control_kind::combo_popup)
                        sibling.visible = open;

            mark_layout_dirty(current);
        }


        static void close_all_combos(session &current)
        {
            if (current.open_combos.empty()) return;

            current.open_combos.clear();
            if (current.tree.has_value())
                apply_combo_visibility(*current.tree, current.open_combos);
            mark_layout_dirty(current);
        }


        [[nodiscard]]
        static auto combo_header(const widget::node_data &node) -> const widget::node_data *
        {
            // Options live inside the popup box, so walk ancestors until one
            // has a combo header as a sibling.
            for (auto *current = &node; current->parent != nullptr; current = current->parent)
                for (const auto &sibling : current->parent->children)
                    if (sibling.control == widget::control_kind::combo_box) return &sibling;

            return nullptr;
        }


        void choose_combo_option(session                 &current,
                                 const widget::node_data &option,
                                 bool                     keyboard = false)
        {
            emit_control(current, event::kind::selection_changed, option, option.text);

            if (const auto *header = combo_header(option); header != nullptr)
            {
                set_combo_open(current, *header, false);
                focus_node(current, *header, keyboard);
            }
            else
                current.dirty = true;
        }


        void close_combo(session &current, const widget::node_data &option, bool keyboard = false)
        {
            if (const auto *header = combo_header(option); header != nullptr)
            {
                set_combo_open(current, *header, false);
                focus_node(current, *header, keyboard);
            }
            else
                current.dirty = true;
        }


        static void close_combos_outside(session &current, const widget::node_data *hit)
        {
            if (current.open_combos.empty()) return;

            for (auto *node = hit; node != nullptr; node = node->parent)
                if (node->control == widget::control_kind::combo_box
                    or node->control == widget::control_kind::combo_popup
                    or node->control == widget::control_kind::combo_option)
                    return;

            current.open_combos.clear();
            if (current.tree) apply_combo_visibility(*current.tree, current.open_combos);
            mark_layout_dirty(current);
        }


        [[nodiscard]]
        static auto visual_line_at(const std::vector<text_line> &lines, std::size_t offset) noexcept
            -> std::size_t
        {
            for (std::size_t i = 0; i + 1 < lines.size(); i++)
                if (offset < lines[i + 1].begin) return i;

            return lines.empty() ? 0 : lines.size() - 1;
        }


        [[nodiscard]]
        static auto text_alignment_for(const widget::node_data &node) noexcept
            -> widget::text_alignment
        {
            auto alignment = widget::text_alignment_of(node);
            if (alignment == widget::text_alignment::justify)
                alignment = widget::text_alignment::left;
            return alignment;
        }


        [[nodiscard]]
        auto line_left(const widget::node_data &node,
                       const text_line         &line,
                       float                    available_width) const noexcept -> float
        {
            return widget::text_alignment_offset(text_alignment_for(node),
                                                 available_width - line.width);
        }


        /// Finds the closest caret offset in \p [begin, end] to \p target,
        /// measured from the start of the line. Prefix widths are monotonic, so
        /// the boundary search is logarithmic after one grapheme walk.
        [[nodiscard]]
        auto closest_offset(render_context          &context,
                            const widget::node_data &node,
                            std::size_t              begin,
                            std::size_t              end,
                            float                    target) const -> std::size_t
        {
            begin = std::min(begin, node.text.size());
            end   = std::min(std::max(begin, end), node.text.size());
            if (target <= 0.F or begin >= end) return begin;

            auto offsets = std::vector<std::size_t> { begin };
            while (offsets.back() < end)
            {
                const auto next = widget::next_offset(node.text, offsets.back());
                if (next <= offsets.back()) break;
                offsets.push_back(std::min(next, end));
            }

            const auto width_at = [&](std::size_t index) -> std::optional<float>
            {
                const auto length = offsets[index] - begin;
                if (length == 0) return 0.F;

                auto width = context.measure_text(
                    std::string_view { node.text }.substr(begin, length), node.font, std::nullopt,
                    widget::text_wrap_mode::no_wrap);
                if (!width) return std::nullopt;
                return width->x;
            };

            auto lo = std::size_t { 0 };
            auto hi = offsets.size() - 1;

            while (lo < hi)
            {
                const auto mid   = (lo + hi + 1) / 2;
                const auto width = width_at(mid);
                if (!width) break;

                if (*width <= target)
                    lo = mid;
                else
                    hi = mid - 1;
            }

            const auto lower = width_at(lo);
            if (!lower) return offsets[lo];

            auto best     = offsets[lo];
            auto distance = std::abs(*lower - target);

            if (lo + 1 < offsets.size())
                if (const auto upper = width_at(lo + 1))
                    if (const auto candidate = std::abs(*upper - target); candidate < distance)
                        best = offsets[lo + 1];

            return best;
        }


        /// Measures the caret's horizontal offset inside one visual line.
        [[nodiscard]]
        auto caret_x_in_line(render_context          &context,
                             const widget::node_data &node,
                             const text_line         &line,
                             std::size_t              offset) const -> float
        {
            const auto end = std::min(line.end, node.text.size());
            const auto at  = std::clamp(offset, line.begin, end);
            if (at <= line.begin) return 0.F;

            auto width = context.measure_text(
                std::string_view { node.text }.substr(line.begin, at - line.begin), node.font,
                std::nullopt, widget::text_wrap_mode::no_wrap);
            return width ? width->x : 0.F;
        }


        [[nodiscard]]
        auto caret_at(render_context          &context,
                      const widget::node_data &node,
                      const math::rect        &bounds,
                      float                    x) const -> std::size_t
        {
            const auto content = bounds.inset(node.padding);
            const auto target  = x - content.position.x;
            return closest_offset(context, node, 0, node.text.size(), target);
        }


        /// Maps a pointer position to a byte offset in a multiline field.
        [[nodiscard]]
        auto multiline_caret_at(render_context          &context,
                                const widget::node_data &node,
                                const math::rect        &bounds,
                                math::vec2               point,
                                float                    scroll_y) const -> std::size_t
        {
            const auto content = bounds.inset(node.padding);
            if (content.size.x <= 0.F or content.size.y <= 0.F) return node.text.size();

            auto lines = context.measure_lines(node.text, node.font, content.size.x, node.wrap);
            if (!lines or lines->empty()) return node.text.size();

            auto line_height = context.line_height(node.font);
            if (!line_height or *line_height <= 0.F) return node.text.size();

            const auto relative_y = point.y - content.position.y + scroll_y;
            const auto total      = *line_height * static_cast<float>(lines->size());
            if (relative_y >= total) return node.text.size();

            const auto index = static_cast<std::size_t>(std::clamp(
                std::floor(relative_y / *line_height), 0.F, static_cast<float>(lines->size() - 1)));

            const auto &line   = (*lines)[index];
            const auto  left   = content.position.x + line_left(node, line, content.size.x);
            const auto  target = point.x - left;

            return closest_offset(context, node, line.begin, line.end, target);
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
            auto       width
                = context.measure_text(std::string_view { node.text }.substr(0, offset), node.font,
                                       std::nullopt, widget::text_wrap_mode::no_wrap);
            if (!width) return 0.F;

            return std::max(0.F, width->x - content.size.x + 2.F);
        }


        /// Scrolls a multiline field just enough to reveal \p caret, then
        /// clamps the offset to the content height.
        auto update_text_scroll(render_context          &context,
                                session                 &current,
                                const widget::node_data &node,
                                const math::rect        &bounds,
                                std::size_t              caret) const -> float
        {
            auto &scroll = current.text_scrolls[node.id];

            const auto content = bounds.inset(node.padding);
            if (content.size.y <= 0.F) return scroll;

            auto lines = context.measure_lines(node.text, node.font, content.size.x, node.wrap);
            if (!lines or lines->empty()) return scroll;

            auto line_height = context.line_height(node.font);
            if (!line_height or *line_height <= 0.F) return scroll;

            const auto index  = visual_line_at(*lines, caret);
            const auto top    = *line_height * static_cast<float>(index);
            const auto bottom = top + *line_height;

            if (top < scroll)
                scroll = top;
            else if (bottom > scroll + content.size.y)
                scroll = bottom - content.size.y;

            const auto total = *line_height * static_cast<float>(lines->size());
            scroll           = std::clamp(scroll, 0.F, std::max(0.F, total - content.size.y));
            return scroll;
        }


        static void copy_selection(const widget::node_data      &node,
                                   const widget::text_selection &edit)
        {
            if (!edit.has_selection()) return;

            const auto text = widget::selected_text(node.text, edit);
            SDL_SetClipboardText(text.c_str());
        }


        /// Inserts the clipboard into \p node when it contains non-empty
        /// text. Returns whether the text changed, so a no-op paste does not
        /// create a spurious undo step.
        auto paste_clipboard(session                &current,
                             widget::node_data      &node,
                             widget::text_selection &edit) const -> bool
        {
            auto *clipboard = SDL_GetClipboardText();
            if (clipboard == nullptr) return false;

            const auto text = node.multiline ? widget::sanitize_multiline(clipboard)
                                             : widget::sanitize_single_line(clipboard);
            SDL_free(clipboard);
            if (text.empty()) return false;

            record_undo(current, node, edit);
            widget::insert_text(node.text, edit, text);
            apply_text_edit(current, node);
            return true;
        }


        void apply_text_edit(session &current, widget::node_data &node) const
        {
            emit_control(current, event::kind::text_edit, node, node.text);
            mark_view_dirty(current);
        }


        static void record_undo(session                      &current,
                                const widget::node_data      &node,
                                const widget::text_selection &edit)
        {
            auto &stack = current.undo[node.id];

            if (!stack.empty() and stack.back().text == node.text
                and stack.back().selection.caret == edit.caret
                and stack.back().selection.anchor == edit.anchor)
                return;

            stack.push_back({ node.text, edit });
            if (stack.size() > 128) stack.erase(stack.begin());

            current.redo[node.id].clear();
        }


        void restore_snapshot(session                &current,
                              widget::node_data      &node,
                              widget::text_selection &edit,
                              bool                    undo) const
        {
            auto &from = undo ? current.undo : current.redo;
            auto &to   = undo ? current.redo : current.undo;

            const auto it = from.find(node.id);
            if (it == from.end() or it->second.empty()) return;

            to[node.id].push_back({ node.text, edit });

            auto snapshot = std::move(it->second.back());
            it->second.pop_back();

            node.text   = std::move(snapshot.text);
            edit        = snapshot.selection;
            edit.caret  = widget::clamp_offset(node.text, edit.caret);
            edit.anchor = widget::clamp_offset(node.text, edit.anchor);

            apply_text_edit(current, node);
        }


        auto handle_text_input_key(session           &current,
                                   render_context    &context,
                                   widget::node_data &node,
                                   const SDL_Event   &native) -> bool
        {
            auto &edit  = current.edits[node.id];
            edit.caret  = widget::clamp_offset(node.text, edit.caret);
            edit.anchor = widget::clamp_offset(node.text, edit.anchor);

            const bool shift   = (native.key.mod & SDL_KMOD_SHIFT) != 0;
            const bool alt     = (native.key.mod & SDL_KMOD_ALT) != 0;
            const bool ctrl    = (native.key.mod & SDL_KMOD_CTRL) != 0;
            const bool gui     = (native.key.mod & SDL_KMOD_GUI) != 0;
            const bool command = ctrl or gui;
            const bool by_word = command or alt;

            const auto *bounds = current.layout ? current.layout->find(node.id) : nullptr;

            switch (native.key.key)
            {
            case SDLK_BACKSPACE:
                if (!edit.has_selection() and edit.caret == 0) return true;
                record_undo(current, node, edit);
                if (by_word ? widget::erase_word_backward(node.text, edit)
                            : widget::erase_backward(node.text, edit))
                    apply_text_edit(current, node);
                current.text_caret_goals.erase(node.id);
                return true;

            case SDLK_DELETE:
                if (!edit.has_selection() and edit.caret >= node.text.size()) return true;
                record_undo(current, node, edit);
                if (by_word ? widget::erase_word_forward(node.text, edit)
                            : widget::erase_forward(node.text, edit))
                    apply_text_edit(current, node);
                current.text_caret_goals.erase(node.id);
                return true;

            case SDLK_LEFT:
                widget::move_caret(node.text, edit,
                                   by_word ? widget::previous_word_offset(node.text, edit.caret)
                                           : widget::previous_offset(node.text, edit.caret),
                                   shift);
                current.text_caret_goals.erase(node.id);
                current.dirty = true;
                return true;

            case SDLK_RIGHT:
                widget::move_caret(node.text, edit,
                                   by_word ? widget::next_word_offset(node.text, edit.caret)
                                           : widget::next_offset(node.text, edit.caret),
                                   shift);
                current.text_caret_goals.erase(node.id);
                current.dirty = true;
                return true;

            case SDLK_UP:
            case SDLK_DOWN:
                {
                    if (!node.multiline) return false;
                    if (bounds == nullptr) return true;

                    const auto content = bounds->inset(node.padding);
                    auto       lines
                        = context.measure_lines(node.text, node.font, content.size.x, node.wrap);
                    if (!lines or lines->empty()) return true;

                    const auto index = visual_line_at(*lines, edit.caret);
                    const auto goal  = [&]
                    {
                        if (const auto it = current.text_caret_goals.find(node.id);
                            it != current.text_caret_goals.end())
                            return it->second;
                        return caret_x_in_line(context, node, (*lines)[index], edit.caret);
                    }();

                    current.text_caret_goals[node.id] = goal;

                    auto target = std::size_t { 0 };
                    if (native.key.key == SDLK_UP)
                    {
                        if (index == 0)
                            target = 0;
                        else
                        {
                            const auto &line = (*lines)[index - 1];
                            target = closest_offset(context, node, line.begin, line.end, goal);
                        }
                    }
                    else if (index + 1 >= lines->size())
                        target = node.text.size();
                    else
                    {
                        const auto &line = (*lines)[index + 1];
                        target = closest_offset(context, node, line.begin, line.end, goal);
                    }

                    widget::move_caret(node.text, edit, target, shift);
                    current.dirty = true;
                    return true;
                }

            case SDLK_HOME:
            case SDLK_END:
                {
                    auto target
                        = native.key.key == SDLK_HOME ? std::size_t { 0 } : node.text.size();

                    if (node.multiline and !command and bounds != nullptr)
                    {
                        const auto content = bounds->inset(node.padding);
                        auto lines = context.measure_lines(node.text, node.font, content.size.x,
                                                           node.wrap);
                        if (lines and !lines->empty())
                        {
                            const auto index = visual_line_at(*lines, edit.caret);
                            target           = native.key.key == SDLK_HOME ? (*lines)[index].begin
                                                                           : (*lines)[index].end;
                        }
                    }

                    widget::move_caret(node.text, edit, target, shift);
                    current.text_caret_goals.erase(node.id);
                    current.dirty = true;
                    return true;
                }

            case SDLK_A:
                if (!command) return false;
                widget::select_all(node.text, edit);
                current.text_caret_goals.erase(node.id);
                current.dirty = true;
                return true;

            case SDLK_C:
                if (!command) return false;
                copy_selection(node, edit);
                return true;

            case SDLK_X:
                if (!command) return false;
                if (edit.has_selection())
                {
                    record_undo(current, node, edit);
                    copy_selection(node, edit);
                    widget::insert_text(node.text, edit, {});
                    apply_text_edit(current, node);
                    current.text_caret_goals.erase(node.id);
                }
                return true;

            case SDLK_V:
                if (!command) return false;
                (void)paste_clipboard(current, node, edit);
                current.text_caret_goals.erase(node.id);
                return true;

            case SDLK_Z:
                if (!command) return false;
                restore_snapshot(current, node, edit, !shift);
                current.text_caret_goals.erase(node.id);
                return true;

            case SDLK_Y:
                if (!command) return false;
                restore_snapshot(current, node, edit, false);
                current.text_caret_goals.erase(node.id);
                return true;

            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                if (node.multiline and !command)
                {
                    record_undo(current, node, edit);
                    widget::insert_text(node.text, edit, "\n");
                    apply_text_edit(current, node);
                    current.text_caret_goals.erase(node.id);
                    return true;
                }

                // A single-line field leaves modified Enter to shortcuts.
                if (!node.multiline and (ctrl or alt or gui)) return false;

                emit_control(current, event::kind::submit, node, node.text);
                return true;

            case SDLK_ESCAPE:
                // Escape cancels an active IME composition before it clears
                // focus; otherwise the field would lose focus mid-composition.
                if (current.preedit_id)
                {
                    SDL_ClearComposition(m_window.get());
                    current.preedit_id.reset();
                    current.preedit_text.clear();
                    current.preedit_caret = 0;
                    current.dirty         = true;
                    return true;
                }

                current.focused.reset();
                current.dirty = true;
                return true;

            default: return false;
            }
        }


        auto handle_slider_key(session &current, widget::node_data &node, const SDL_Event &native)
            -> bool
        {
            if ((native.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) != 0) return false;

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


        /// The nearest list or tree surface that owns \p node, if any.
        [[nodiscard]]
        static auto row_container(const widget::node_data &node) noexcept
            -> const widget::node_data *
        {
            for (const auto *ancestor = node.parent; ancestor != nullptr;
                 ancestor             = ancestor->parent)
                if (ancestor->control == widget::control_kind::list_view
                    or ancestor->control == widget::control_kind::tree_view)
                    return ancestor;

            return nullptr;
        }


        /// Collects visible rows that belong directly to \p node. Nested list
        /// and tree surfaces own their own rows and are not descended into.
        template <typename Node>
        static void collect_list_rows(Node &node, std::vector<Node *> &rows)
        {
            for (auto &child :
                 node.children
                     | std::views::filter([](const auto &value)
                                          { return value.visible and !value.disabled; }))
            {
                if (child.control == widget::control_kind::list_view
                    or child.control == widget::control_kind::tree_view)
                    continue;

                if (child.control == widget::control_kind::list_item
                    or child.control == widget::control_kind::tree_item)
                {
                    rows.push_back(&child);
                    continue;
                }

                collect_list_rows(child, rows);
            }
        }


        [[nodiscard]]
        static auto row_index(const std::vector<const widget::node_data *> &rows,
                              const widget::node_data &row) -> std::optional<std::size_t>
        {
            const auto found = std::ranges::find_if(rows, [&row](const auto *candidate)
                                                    { return candidate->id == row.id; });
            if (found == rows.end()) return std::nullopt;
            return static_cast<std::size_t>(found - rows.begin());
        }


        /// Stable identity used by runtime-managed multi-selection.
        [[nodiscard]]
        static auto row_key(const widget::node_data &row) -> std::string
        { return row.action.empty() ? row.text : row.action; }


        [[nodiscard]]
        static auto contains_key(const std::vector<std::string> &keys, std::string_view key) -> bool
        {
            return std::ranges::find_if(keys, [key](const auto &value) { return value == key; })
                != keys.end();
        }


        static void erase_key(std::vector<std::string> &keys, std::string_view key)
        {
            std::erase_if(keys, [key](const auto &value) { return value == key; });
        }


        /// Orders selected keys by their visible row order. Keys that do not
        /// match a visible row are kept at the end so collapsing a branch does
        /// not discard its selection.
        [[nodiscard]]
        static auto order_selection(const std::vector<widget::node_data *> &rows,
                                    const std::vector<std::string>         &keys)
            -> std::vector<std::string>
        {
            auto wanted = std::unordered_set<std::string> { keys.begin(), keys.end() };
            auto seen   = std::unordered_set<std::string> {};
            auto result = std::vector<std::string> {};
            result.reserve(keys.size());

            for (const auto *row : rows)
            {
                auto key = row_key(*row);
                if (!wanted.contains(key)) continue;
                if (!seen.insert(key).second) continue;
                result.push_back(std::move(key));
            }

            for (const auto &key : keys)
                if (seen.insert(key).second) result.push_back(key);

            return result;
        }


        /// Applies the runtime-owned selection to a freshly built tree. A
        /// container's first frame seeds its selection from rows that already
        /// carry \c selected, so an application can provide an initial value.
        void apply_multi_selection(widget::node_data &node)
        {
            if (node.multi_select)
            {
                auto rows = std::vector<widget::node_data *> {};
                collect_list_rows(node, rows);

                auto &state = m_selections[node.id];

                if (!state.initialized)
                {
                    state.initialized = true;

                    for (const auto *row : rows)
                        if (row->selected) state.rows.push_back(row_key(*row));

                    if (!state.rows.empty()) state.anchor = state.rows.front();
                }

                state.rows = order_selection(rows, state.rows);

                for (auto *row : rows) row->selected = contains_key(state.rows, row_key(*row));
            }

            for (auto &child : node.children) apply_multi_selection(child);
        }


        void emit_selection(session                     &current,
                            const widget::node_data     &row,
                            std::uint16_t                modifiers,
                            const multi_selection_state &state) const
        {
            event result;
            result.type      = event::kind::selection_changed;
            result.target    = row.id;
            result.action    = row.action;
            result.text      = row.text;
            result.modifiers = modifiers;
            result.selection = state.rows;
            dispatch(result, current);
        }


        /// Updates a multi-select container from one row click. A plain click
        /// replaces the selection, Shift extends from the anchor, and
        /// Ctrl/Cmd toggles the row. Ctrl+Shift unions a range.
        void select_row(session &current, widget::node_data &row, std::uint16_t modifiers)
        {
            const auto *container = row_container(row);
            if (container == nullptr or !container->multi_select) return;

            auto *editable = current.tree ? current.tree->find(container->id) : nullptr;
            if (editable == nullptr) return;

            auto rows = std::vector<widget::node_data *> {};
            collect_list_rows(*editable, rows);

            auto &state = m_selections[container->id];

            if (!state.initialized)
            {
                state.initialized = true;

                for (const auto *candidate : rows)
                    if (candidate->selected) state.rows.push_back(row_key(*candidate));

                if (!state.rows.empty()) state.anchor = state.rows.front();
            }

            const auto key   = row_key(row);
            const auto ctrl  = (modifiers & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
            const auto shift = (modifiers & SDL_KMOD_SHIFT) != 0;

            if (shift and state.anchor)
            {
                const auto anchor
                    = std::ranges::find_if(rows, [&state](const auto *candidate)
                                           { return row_key(*candidate) == *state.anchor; });
                const auto clicked = std::ranges::find_if(rows, [&key](const auto *candidate)
                                                          { return row_key(*candidate) == key; });

                if (anchor != rows.end() and clicked != rows.end())
                {
                    const auto first = std::min(anchor, clicked);
                    const auto last  = std::max(anchor, clicked);

                    if (!ctrl) state.rows.clear();

                    for (auto it = first; it != last + 1; ++it)
                        if (!contains_key(state.rows, row_key(**it)))
                            state.rows.push_back(row_key(**it));
                }
                else
                {
                    state.rows   = { key };
                    state.anchor = key;
                }
            }
            else if (ctrl)
            {
                if (contains_key(state.rows, key))
                    erase_key(state.rows, key);
                else
                    state.rows.push_back(key);

                state.anchor = key;
            }
            else
            {
                state.rows   = { key };
                state.anchor = key;
            }

            state.rows = order_selection(rows, state.rows);

            for (auto *candidate : rows)
                candidate->selected = contains_key(state.rows, row_key(*candidate));

            emit_selection(current, row, modifiers, state);
            current.dirty = true;
        }


        /// Ctrl+Space toggles the focused row without moving focus.
        void toggle_row_selection(session &current, widget::node_data &row)
        { select_row(current, row, static_cast<std::uint16_t>(SDL_KMOD_CTRL)); }


        [[nodiscard]]
        static auto multi_select_row(const widget::node_data *hit) -> bool
        {
            const auto *target = _impl::control_target(hit);
            if (target == nullptr) return false;
            if (target->control != widget::control_kind::list_item
                and target->control != widget::control_kind::tree_item)
                return false;

            const auto *container = row_container(*target);
            return container != nullptr and container->multi_select;
        }


        /// How many rows fit in the viewport. Used by PageUp and PageDown.
        [[nodiscard]]
        auto page_step(const session           &current,
                       const widget::node_data &container,
                       const widget::node_data &row) const -> std::ptrdiff_t
        {
            constexpr auto fallback = std::ptrdiff_t { 10 };
            if (!current.layout) return fallback;

            const auto *viewport = current.layout->find(container.id);
            const auto *bounds   = current.layout->find(row.id);
            if (viewport == nullptr or bounds == nullptr or bounds->size.y <= 0.F) return fallback;

            const auto visible = viewport->size.y / bounds->size.y;
            return std::max(std::ptrdiff_t { 1 }, static_cast<std::ptrdiff_t>(visible));
        }


        /// Moves focus by \p delta within the rows of the nearest list or tree
        /// surface. Edges clamp instead of wrapping, matching a list box.
        auto move_row_focus(session                 &current,
                            const widget::node_data &row,
                            std::ptrdiff_t           delta,
                            bool                     edge = false,
                            bool                     last = false) -> bool
        {
            const auto *container = row_container(row);
            if (container == nullptr) return false;

            auto rows = std::vector<const widget::node_data *> {};
            collect_list_rows(*container, rows);
            const auto index = row_index(rows, row);
            if (!index or rows.empty()) return false;

            auto next = std::ptrdiff_t { 0 };
            if (edge)
                next = last ? static_cast<std::ptrdiff_t>(rows.size()) - 1 : 0;
            else
                next = std::clamp(static_cast<std::ptrdiff_t>(*index) + delta, std::ptrdiff_t { 0 },
                                  static_cast<std::ptrdiff_t>(rows.size()) - 1);

            if (static_cast<std::size_t>(next) == *index) return true;

            focus_node(current, *rows[static_cast<std::size_t>(next)], true);
            return true;
        }


        void toggle_tree_row(session &current, widget::node_data &node)
        {
            const auto action
                = node.tree_toggle_action.empty() ? node.action : node.tree_toggle_action;
            if (action.empty()) return;

            emit_action(current, event::kind::activate, node, action, node.text);
        }


        auto handle_list_item_key(session           &current,
                                  widget::node_data &node,
                                  const SDL_Event   &native) -> bool
        {
            if (const auto *container = row_container(node);
                container != nullptr and container->multi_select)
            {
                if (native.key.key == SDLK_SPACE
                    and (native.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0)
                {
                    if (!native.key.repeat) toggle_row_selection(current, node);
                    return true;
                }

                if (native.key.key == SDLK_RETURN or native.key.key == SDLK_KP_ENTER
                    or native.key.key == SDLK_SPACE)
                {
                    if (!native.key.repeat)
                        select_row(current, node, static_cast<std::uint16_t>(native.key.mod));
                    return true;
                }
            }

            if ((native.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) != 0) return false;

            switch (native.key.key)
            {
            case SDLK_UP:   return move_row_focus(current, node, -1);
            case SDLK_DOWN: return move_row_focus(current, node, 1);
            case SDLK_HOME: return move_row_focus(current, node, 0, true, false);
            case SDLK_END:  return move_row_focus(current, node, 0, true, true);

            case SDLK_PAGEUP:
            case SDLK_PAGEDOWN:
                {
                    const auto *container = row_container(node);
                    if (container == nullptr) return false;
                    const auto step = page_step(current, *container, node);
                    return move_row_focus(current, node,
                                          native.key.key == SDLK_PAGEDOWN ? step : -step);
                }

            // A list is vertical. Consuming horizontal arrows keeps focus from
            // escaping through the enclosing focus group.
            case SDLK_LEFT:
            case SDLK_RIGHT: return true;

            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_SPACE:
                if (native.key.repeat) return true;
                emit_control(current, event::kind::activate, node, node.text);
                return true;

            default: return false;
            }
        }


        auto handle_tree_item_key(session           &current,
                                  widget::node_data &node,
                                  const SDL_Event   &native) -> bool
        {
            if (const auto *container = row_container(node);
                container != nullptr and container->multi_select)
            {
                if (native.key.key == SDLK_SPACE
                    and (native.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0)
                {
                    if (!native.key.repeat) toggle_row_selection(current, node);
                    return true;
                }

                if (native.key.key == SDLK_RETURN or native.key.key == SDLK_KP_ENTER
                    or native.key.key == SDLK_SPACE)
                {
                    if (!native.key.repeat)
                        select_row(current, node, static_cast<std::uint16_t>(native.key.mod));
                    return true;
                }
            }

            if ((native.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) != 0) return false;

            switch (native.key.key)
            {
            case SDLK_UP:   return move_row_focus(current, node, -1);
            case SDLK_DOWN: return move_row_focus(current, node, 1);
            case SDLK_HOME: return move_row_focus(current, node, 0, true, false);
            case SDLK_END:  return move_row_focus(current, node, 0, true, true);

            case SDLK_PAGEUP:
            case SDLK_PAGEDOWN:
                {
                    const auto *container = row_container(node);
                    if (container == nullptr) return false;
                    const auto step = page_step(current, *container, node);
                    return move_row_focus(current, node,
                                          native.key.key == SDLK_PAGEDOWN ? step : -step);
                }

            case SDLK_RIGHT:
                {
                    if (!node.tree_has_children) return true;
                    if (!node.tree_expanded)
                    {
                        toggle_tree_row(current, node);
                        return true;
                    }

                    const auto *container = row_container(node);
                    if (container == nullptr) return true;

                    auto rows = std::vector<const widget::node_data *> {};
                    collect_list_rows(*container, rows);
                    const auto index = row_index(rows, node);
                    if (index and *index + 1 < rows.size()
                        and rows[*index + 1]->tree_depth > node.tree_depth)
                        focus_node(current, *rows[*index + 1], true);
                    return true;
                }

            case SDLK_LEFT:
                {
                    if (node.tree_has_children and node.tree_expanded)
                    {
                        toggle_tree_row(current, node);
                        return true;
                    }

                    const auto *container = row_container(node);
                    if (container == nullptr) return true;

                    auto rows = std::vector<const widget::node_data *> {};
                    collect_list_rows(*container, rows);
                    const auto index = row_index(rows, node);
                    if (!index) return true;

                    auto parents
                        = rows | std::views::take(*index) | std::views::reverse
                        | std::views::filter([&node](const auto *candidate)
                                             { return candidate->tree_depth < node.tree_depth; });
                    if (!std::ranges::empty(parents))
                        focus_node(current, **std::ranges::begin(parents), true);
                    return true;
                }

            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_SPACE:
                if (native.key.repeat) return true;
                emit_control(current, event::kind::activate, node, node.text);
                return true;

            default: return false;
            }
        }


        static void collect_combo_options(const widget::node_data                &node,
                                          std::vector<const widget::node_data *> &options)
        {
            for (const auto &child : node.children)
            {
                if (child.control == widget::control_kind::combo_option)
                {
                    if (child.visible and !child.disabled and _impl::effectively_visible(child))
                        options.push_back(&child);
                    continue;
                }

                collect_combo_options(child, options);
            }
        }


        [[nodiscard]]
        static auto combo_options(const widget::node_data &header)
            -> std::vector<const widget::node_data *>
        {
            auto options = std::vector<const widget::node_data *> {};
            if (header.parent == nullptr) return options;

            collect_combo_options(*header.parent, options);
            return options;
        }


        /// Moves focus by \p delta through the visible options of a combo box.
        /// When the header currently owns focus, a positive delta starts at
        /// the first option and a negative delta at the last.
        void focus_combo_option(session &current, const widget::node_data &header, int delta)
        {
            const auto options = combo_options(header);
            if (options.empty()) return;

            auto index = std::size_t { 0 };
            if (const auto it
                = std::find_if(options.begin(), options.end(), [&current](const auto *option)
                               { return current.focused and option->id == *current.focused; });
                it != options.end())
            {
                const auto current_index = static_cast<std::ptrdiff_t>(it - options.begin());
                index                    = static_cast<std::size_t>(
                    std::clamp(current_index + delta, std::ptrdiff_t { 0 },
                               static_cast<std::ptrdiff_t>(options.size()) - 1));
            }
            else
                index = delta < 0 ? options.size() - 1 : 0;

            focus_node(current, *options[index], true);
        }


        /// Focuses the first or last visible option, ignoring the current
        /// focus. Home and End use this instead of a large relative delta,
        /// which would be ambiguous when the header owns focus.
        void focus_combo_edge(session &current, const widget::node_data &header, bool last)
        {
            const auto options = combo_options(header);
            if (options.empty()) return;

            focus_node(current, last ? *options.back() : *options.front(), true);
        }


        auto handle_combo_key(session &current, widget::node_data &node, const SDL_Event &native)
            -> bool
        {
            if ((native.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) != 0) return false;

            const bool is_open = current.open_combos.contains(node.id);

            switch (native.key.key)
            {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_SPACE:
                if (native.key.repeat) return true;
                set_combo_open(current, node, !is_open);
                return true;

            case SDLK_ESCAPE:
                if (!is_open) return false;
                set_combo_open(current, node, false);
                return true;

            case SDLK_DOWN:
                if (!is_open)
                {
                    set_combo_open(current, node, true);
                    focus_combo_option(current, node, 1);
                }
                else
                    focus_combo_option(current, node, 1);
                return true;

            case SDLK_UP:
                if (!is_open)
                {
                    set_combo_open(current, node, true);
                    focus_combo_option(current, node, -1);
                }
                else
                    focus_combo_option(current, node, -1);
                return true;

            case SDLK_HOME:
                if (!is_open) return false;
                focus_combo_edge(current, node, false);
                return true;

            case SDLK_END:
                if (!is_open) return false;
                focus_combo_edge(current, node, true);
                return true;

            default: return false;
            }
        }


        auto handle_combo_option_key(session           &current,
                                     widget::node_data &node,
                                     const SDL_Event   &native) -> bool
        {
            if ((native.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) != 0) return false;

            switch (native.key.key)
            {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_SPACE:
                if (native.key.repeat) return true;
                choose_combo_option(current, node, true);
                return true;

            case SDLK_ESCAPE: close_combo(current, node, true); return true;

            case SDLK_DOWN:
            case SDLK_UP:
                if (const auto *header = combo_header(node); header != nullptr)
                    focus_combo_option(current, *header, native.key.key == SDLK_DOWN ? 1 : -1);
                return true;

            case SDLK_HOME:
            case SDLK_END:
                if (const auto *header = combo_header(node); header != nullptr)
                    focus_combo_edge(current, *header, native.key.key == SDLK_END);
                return true;

            default: return false;
            }
        }


        auto handle_control_key(session &current, render_context &context, const SDL_Event &native)
            -> bool
        {
            auto *node = focused_node(current);
            if (node == nullptr or !_impl::effectively_visible(*node)) return false;

            switch (node->control)
            {
            case widget::control_kind::text_input:
                return handle_text_input_key(current, context, *node, native);
            case widget::control_kind::slider:    return handle_slider_key(current, *node, native);
            case widget::control_kind::combo_box: return handle_combo_key(current, *node, native);
            case widget::control_kind::combo_option:
                return handle_combo_option_key(current, *node, native);
            case widget::control_kind::list_item:
                return handle_list_item_key(current, *node, native);
            case widget::control_kind::tree_item:
                return handle_tree_item_key(current, *node, native);
            case widget::control_kind::list_view:
            case widget::control_kind::tree_view:
            case widget::control_kind::combo_popup:
            case widget::control_kind::checkbox_indicator:
            case widget::control_kind::radio_indicator:
            case widget::control_kind::none:               break;
            }

            if (node->action.empty()) return false;

            // Modified Enter and Space are left to shortcuts; keyboard
            // activation uses the unmodified keys.
            if ((native.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) != 0) return false;

            switch (native.key.key)
            {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_SPACE:
                if (native.key.repeat) return true;
                emit_control(current, event::kind::activate, *node);
                return true;
            default: return false;
            }
        }


        auto handle_focus_key(session &current, const SDL_Event &native) -> bool
        {
            if (native.key.key != SDLK_TAB) return false;
            if ((native.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) != 0) return false;
            if (native.key.repeat) return true;

            // Tab dismisses an open popup. Focus returns to its header before
            // traversal continues so the next Tab moves to the following
            // widget instead of an option that just disappeared.
            if (!current.open_combos.empty())
            {
                if (auto *focused = focused_node(current);
                    focused != nullptr and focused->control == widget::control_kind::combo_option)
                    if (const auto *header = combo_header(*focused); header != nullptr)
                        focus_node(current, *header, true);

                close_all_combos(current);
            }

            focus_next(current, (native.key.mod & SDL_KMOD_SHIFT) != 0);
            return true;
        }


        void update_focus_from_hit(session &current, const widget::node_data *hit)
        {
            const auto *target = _impl::focus_target(hit);
            if (target == nullptr)
            {
                // Clicking the popup's padding should leave focus where it is
                // instead of dropping the combo header focus.
                if (_impl::inside_combo_popup(hit)) return;

                // Clicking a modal surface or the scrim keeps focus inside the
                // modal until it closes and restores the saved focus.
                if (current.active_modal) return;

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

            // The thumb travels inside the track, so its center maps to the
            // value instead of the outer bounds.
            const auto radius = std::min(widget::slider_thumb_radius,
                                         std::min(bounds->size.x, bounds->size.y) / 2.F);
            const auto travel = std::max(1.F, bounds->size.x - (2.F * radius));
            const auto value  = std::clamp((x - bounds->position.x - radius) / travel, 0.F, 1.F);
            if (value == node.control_value) return;

            node.control_value = value;
            emit_control(current, event::kind::slider_changed, node, {}, value);
            current.dirty = true;
        }


        void handle_control_press(session                 &current,
                                  render_context          &context,
                                  const widget::node_data *hit,
                                  math::vec2               point,
                                  int                      clicks = 1)
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

                    auto caret = node->text.size();
                    if (const auto *bounds
                        = current.layout ? current.layout->find(node->id) : nullptr)
                    {
                        if (node->multiline)
                        {
                            const auto it     = current.text_scrolls.find(node->id);
                            const auto scroll = it != current.text_scrolls.end() ? it->second : 0.F;
                            caret = multiline_caret_at(context, *node, *bounds, point, scroll);
                        }
                        else
                        {
                            const auto scroll = text_scroll(context, *node, *bounds, edit.caret);
                            caret             = caret_at(context, *node, *bounds, point.x + scroll);
                        }
                    }

                    current.text_caret_goals.erase(node->id);

                    if (clicks >= 3)
                        widget::select_all(node->text, edit);
                    else if (clicks == 2)
                    {
                        const auto [first, last] = widget::word_range_at(node->text, caret);
                        edit.anchor              = first;
                        edit.caret               = last;
                    }
                    else
                        edit.collapse(caret);

                    current.selecting = node->id;
                    current.dirty     = true;
                    break;
                }

            case widget::control_kind::slider:
                focus_node(current, *node);
                current.dragging = node->id;
                update_slider(current, *node, point.x);
                break;

            case widget::control_kind::combo_box:
                focus_node(current, *node);
                set_combo_open(current, *node, !current.open_combos.contains(node->id));
                break;

            case widget::control_kind::combo_option: choose_combo_option(current, *node); break;

            case widget::control_kind::list_item:
                {
                    focus_node(current, *node);

                    const auto *container = row_container(*node);
                    if (container != nullptr and container->multi_select)
                        select_row(current, *node, static_cast<std::uint16_t>(SDL_GetModState()));
                    else
                        emit_control(current, event::kind::activate, *node, node->text);
                    break;
                }

            case widget::control_kind::tree_item:
                {
                    focus_node(current, *node);

                    auto disclosure = false;
                    if (node->tree_has_children)
                        if (const auto *bounds
                            = current.layout ? current.layout->find(node->id) : nullptr)
                        {
                            const auto left = bounds->position.x + widget::tree_row_inset
                                            + (static_cast<float>(node->tree_depth)
                                               * widget::tree_indent_width);
                            disclosure
                                = point.x >= left and point.x < left + widget::tree_chevron_width;
                        }

                    const auto *container = row_container(*node);

                    if (disclosure)
                        toggle_tree_row(current, *node);
                    else if (container != nullptr and container->multi_select)
                        select_row(current, *node, static_cast<std::uint16_t>(SDL_GetModState()));
                    else
                        emit_control(current, event::kind::activate, *node, node->text);
                    break;
                }

            case widget::control_kind::list_view:
            case widget::control_kind::tree_view:
            case widget::control_kind::combo_popup:
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
                        const auto caret
                            = node->multiline
                                ? multiline_caret_at(context, *node, *bounds, point,
                                                     current.text_scrolls.contains(node->id)
                                                         ? current.text_scrolls[node->id]
                                                         : 0.F)
                                : caret_at(context, *node, *bounds, point.x + scroll);

                        widget::move_caret(node->text, edit, caret, true);
                        current.text_caret_goals.erase(node->id);
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

            if (current.scrollbar_drag) update_scrollbar_drag(current, point);
            if (current.panning) update_pan(current, point);

            handle_control_motion(current, context, point);

            update_cursor(current, hit, point);

            current.pointer_target
                = hit != nullptr ? std::optional<widget::widget_id> { hit->id } : std::nullopt;
            restart_tooltip(current);

            auto next_hover = visual_target(hit);
            if (current.hovered != next_hover)
            {
                current.hovered = std::move(next_hover);
                current.dirty   = true;
            }

            auto value      = make_event(event::kind::mouse_move, hit);
            value.x         = point.x;
            value.y         = point.y;
            value.modifiers = static_cast<std::uint16_t>(SDL_GetModState());
            dispatch(value, current);
        }


        void
        handle_pointer_button(session &current, render_context &context, const SDL_Event &native)
        {
            const bool  is_down = native.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
            const auto *hit     = hit_at(current, native.button.x, native.button.y);
            const auto  point   = to_logical(native.button.x, native.button.y);

            if (is_down)
            {
                hide_tooltip(current);
                current.tooltip_hover.reset();
            }

            // The middle button pans the nearest scrollable view. Starting
            // from the visible offset avoids a jump when an eased wheel scroll
            // is still in flight.
            if (native.button.button == SDL_BUTTON_MIDDLE)
            {
                if (is_down)
                {
                    if (const auto *scroller = scroll_target(hit); scroller != nullptr)
                        current.panning =
                            typename session::pan_state { scroller->id, point,
                                                          current.scrolls[scroller->id] };
                }
                else
                    current.panning.reset();
            }

            update_cursor(current, hit, point);

            // Scrollbar gestures consume the left button before normal control
            // handling so clicking a scrollbar never selects the row beneath.
            if (native.button.button == SDL_BUTTON_LEFT)
            {
                if (is_down)
                {
                    if (const auto bar = scrollbar_at(current, hit, point))
                    {
                        begin_scrollbar_drag(current, *bar, point);
                        update_cursor(current, hit, point);

                        auto value      = make_event(event::kind::mouse_down, bar->scroller);
                        value.x         = point.x;
                        value.y         = point.y;
                        value.button    = native.button.button;
                        value.modifiers = static_cast<std::uint16_t>(SDL_GetModState());
                        dispatch(value, current);
                        return;
                    }
                }
                else if (current.scrollbar_drag)
                {
                    const auto scroller_id = current.scrollbar_drag->scroller;
                    current.scrollbar_drag.reset();
                    update_cursor(current, hit, point);

                    auto *scroller = current.tree ? current.tree->find(scroller_id) : nullptr;

                    auto value      = make_event(event::kind::mouse_up, scroller);
                    value.x         = point.x;
                    value.y         = point.y;
                    value.button    = native.button.button;
                    value.modifiers = static_cast<std::uint16_t>(SDL_GetModState());
                    dispatch(value, current);
                    return;
                }
            }

            if (native.button.button == SDL_BUTTON_LEFT and is_down)
            {
                close_combos_outside(current, hit);
                update_focus_from_hit(current, hit);
                handle_control_press(current, context, hit, point,
                                     static_cast<int>(native.button.clicks));
            }

            auto value = make_event(is_down ? event::kind::mouse_down : event::kind::mouse_up, hit);
            value.x    = point.x;
            value.y    = point.y;
            value.button    = native.button.button;
            value.modifiers = static_cast<std::uint16_t>(SDL_GetModState());

            if (native.button.button == SDL_BUTTON_LEFT and !is_down)
            {
                if (!release_matches_press(current, hit)) value.action.clear();

                // Multi-select rows report a selection event instead of their
                // click action. Keyboard activation still uses the action.
                if (multi_select_row(hit)) value.action.clear();
            }

            // Clicking the modal surface itself (its padding or scrim) cancels
            // it. Clicks on child content do not bubble into this path.
            if (native.button.button == SDL_BUTTON_LEFT and !is_down and hit != nullptr
                and hit->modal and hit->id == current.active_modal and !hit->cancel_action.empty())
                value.action = hit->cancel_action;

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


        [[nodiscard]]
        static auto scroll_target(const widget::node_data *hit) noexcept
            -> const widget::node_data *
        {
            for (auto *current = hit; current != nullptr; current = current->parent)
                if (current->scrollable) return current;
            return nullptr;
        }


        /// Finds the scrollbar under \p point, if any. Thumbs take precedence
        /// over tracks, and the nearest scrollable ancestor owns the hit.
        [[nodiscard]]
        auto scrollbar_at(const session           &current,
                          const widget::node_data *hit,
                          math::vec2               point) const -> std::optional<scrollbar_hit>
        {
            const auto *scroller = scroll_target(hit);
            if (scroller == nullptr or !current.layout) return std::nullopt;

            const auto *bounds  = current.layout->find(scroller->id);
            const auto  metrics = current.scroll_metrics.find(scroller->id);
            if (bounds == nullptr or metrics == current.scroll_metrics.end()) return std::nullopt;

            const auto bars = widget::scrollbar_layout_for(*bounds, metrics->second);

            if (bars.vertical.visible and bars.vertical.thumb.contains(point))
                return scrollbar_hit { scroller, false, true };
            if (bars.horizontal.visible and bars.horizontal.thumb.contains(point))
                return scrollbar_hit { scroller, true, true };
            if (bars.vertical.visible and bars.vertical.track.contains(point))
                return scrollbar_hit { scroller, false, false };
            if (bars.horizontal.visible and bars.horizontal.track.contains(point))
                return scrollbar_hit { scroller, true, false };

            return std::nullopt;
        }


        void set_scroll_axis(session                 &current,
                             const widget::node_data &scroller,
                             bool                     horizontal,
                             float                    value)
        {
            if (const auto metrics = current.scroll_metrics.find(scroller.id);
                metrics != current.scroll_metrics.end())
                value = std::clamp(value, 0.F,
                                   horizontal ? metrics->second.max.x : metrics->second.max.y);
            else
                value = std::max(0.F, value);

            auto &target = current.scroll_targets[scroller.id];
            auto &offset = current.scrolls[scroller.id];

            if (horizontal)
            {
                target.x = value;
                offset.x = value;
            }
            else
            {
                target.y = value;
                offset.y = value;
            }

            mark_layout_dirty(current);
        }


        /// Starts a scrollbar thumb drag. Clicking the track centers the thumb
        /// on the pointer and then continues as a drag, so click-and-drag uses
        /// one continuous gesture.
        void begin_scrollbar_drag(session &current, const scrollbar_hit &bar, math::vec2 point)
        {
            if (bar.scroller == nullptr or !current.layout) return;

            const auto *bounds  = current.layout->find(bar.scroller->id);
            const auto  metrics = current.scroll_metrics.find(bar.scroller->id);
            if (bounds == nullptr or metrics == current.scroll_metrics.end()) return;

            const auto  bars = widget::scrollbar_layout_for(*bounds, metrics->second);
            const auto &axis = bar.horizontal ? bars.horizontal : bars.vertical;
            if (!axis.visible) return;

            const auto length       = bar.horizontal ? axis.track.size.x : axis.track.size.y;
            const auto thumb_length = bar.horizontal ? axis.thumb.size.x : axis.thumb.size.y;
            const auto travel       = length - thumb_length;
            if (travel <= 0.F) return;

            const auto position = bar.horizontal ? point.x - axis.track.position.x
                                                 : point.y - axis.track.position.y;
            const auto grab     = bar.on_thumb ? (bar.horizontal ? point.x - axis.thumb.position.x
                                                                 : point.y - axis.thumb.position.y)
                                               : thumb_length / 2.F;

            current.scrollbar_drag =
                typename session::scrollbar_drag_state { bar.scroller->id, bar.horizontal, grab };

            const auto maximum = bar.horizontal ? metrics->second.max.x : metrics->second.max.y;
            const auto amount  = std::clamp((position - grab) / travel, 0.F, 1.F);
            set_scroll_axis(current, *bar.scroller, bar.horizontal, amount * maximum);
        }


        void update_scrollbar_drag(session &current, math::vec2 point)
        {
            if (!current.scrollbar_drag or !current.layout) return;

            const auto  drag     = *current.scrollbar_drag;
            auto       *scroller = current.tree ? current.tree->find(drag.scroller) : nullptr;
            const auto *bounds   = current.layout->find(drag.scroller);
            const auto  metrics  = current.scroll_metrics.find(drag.scroller);
            if (scroller == nullptr or bounds == nullptr or metrics == current.scroll_metrics.end())
                return;

            const auto  bars = widget::scrollbar_layout_for(*bounds, metrics->second);
            const auto &axis = drag.horizontal ? bars.horizontal : bars.vertical;
            if (!axis.visible) return;

            const auto length       = drag.horizontal ? axis.track.size.x : axis.track.size.y;
            const auto thumb_length = drag.horizontal ? axis.thumb.size.x : axis.thumb.size.y;
            const auto travel       = length - thumb_length;
            if (travel <= 0.F) return;

            const auto position = drag.horizontal ? point.x - axis.track.position.x
                                                  : point.y - axis.track.position.y;
            const auto maximum  = drag.horizontal ? metrics->second.max.x : metrics->second.max.y;
            const auto amount   = std::clamp((position - drag.grab) / travel, 0.F, 1.F);

            set_scroll_axis(current, *scroller, drag.horizontal, amount * maximum);
        }


        /// Pans the viewport under a middle-button drag. Dragging right moves
        /// content right, so the scroll offset moves in the opposite direction.
        void update_pan(session &current, math::vec2 point)
        {
            if (!current.panning) return;

            const auto pan      = *current.panning;
            auto      *scroller = current.tree ? current.tree->find(pan.scroller) : nullptr;
            if (scroller == nullptr)
            {
                current.panning.reset();
                return;
            }

            const auto delta = point - pan.origin;
            auto       value = math::vec2 { pan.offset.x - delta.x, pan.offset.y - delta.y };

            if (const auto metrics = current.scroll_metrics.find(pan.scroller);
                metrics != current.scroll_metrics.end())
            {
                value.x = std::clamp(value.x, 0.F, metrics->second.max.x);
                value.y = std::clamp(value.y, 0.F, metrics->second.max.y);
            }
            else
            {
                value.x = std::max(0.F, value.x);
                value.y = std::max(0.F, value.y);
            }

            current.scroll_targets[pan.scroller] = value;
            current.scrolls[pan.scroller]        = value;
            mark_layout_dirty(current);
        }


        static void accumulate_content(const widget::node_data     &node,
                                       const widget::layout_result &layout,
                                       std::optional<math::rect>   &content)
        {
            for (const auto &child : node.children)
            {
                if (!child.visible or child.floating) continue;

                if (const auto *bounds = layout.find(child.id))
                {
                    if (!content)
                        content = *bounds;
                    else
                    {
                        const auto left   = std::min(content->position.x, bounds->position.x);
                        const auto top    = std::min(content->position.y, bounds->position.y);
                        const auto right  = std::max(content->right(), bounds->right());
                        const auto bottom = std::max(content->bottom(), bounds->bottom());
                        content           = math::rect { left, top, right - left, bottom - top };
                    }
                }

                accumulate_content(child, layout, content);
            }
        }


        static void translate_subtree(const widget::node_data &node,
                                      widget::layout_result   &layout,
                                      math::vec2               delta)
        {
            for (const auto &child : node.children)
            {
                if (const auto it = layout.rects.find(child.id); it != layout.rects.end())
                    it->second.position += delta;

                translate_subtree(child, layout, delta);
            }
        }


        /// Scrolls the focused widget into view inside its scrollable
        /// ancestors. Only runs after a focus change so wheel scrolling is not
        /// immediately undone.
        void ensure_visible(session &current, const widget::layout_result &layout)
        {
            if (!current.reveal_focus) return;
            current.reveal_focus = false;

            if (!current.focused or !current.tree) return;

            const auto *focused = current.tree->find(*current.focused);
            if (focused == nullptr or !focused->reveal_on_focus) return;

            const auto *target = layout.find(focused->id);
            if (target == nullptr) return;

            for (auto *ancestor = focused->parent; ancestor != nullptr; ancestor = ancestor->parent)
            {
                if (!ancestor->scrollable) continue;

                const auto *viewport = layout.find(ancestor->id);
                if (viewport == nullptr) continue;

                auto &offset = current.scroll_targets[ancestor->id];

                const auto left   = viewport->position.x + ancestor->padding.left;
                const auto top    = viewport->position.y + ancestor->padding.top;
                const auto right  = viewport->right() - ancestor->padding.right;
                const auto bottom = viewport->bottom() - ancestor->padding.bottom;

                if (target->position.x < left + offset.x)
                    offset.x = target->position.x - left;
                else if (target->right() > right + offset.x)
                    offset.x = target->right() - right;

                if (target->position.y < top + offset.y)
                    offset.y = target->position.y - top;
                else if (target->bottom() > bottom + offset.y)
                    offset.y = target->bottom() - bottom;

                offset.x = std::max(0.F, offset.x);
                offset.y = std::max(0.F, offset.y);
            }
        }


        /// Applies the stored wheel offsets to a freshly calculated layout.
        /// Nested scroll containers are resolved bottom-up so an outer viewport
        /// also moves the already-panned inner content.
        auto apply_scroll_offsets(session                 &current,
                                  const widget::node_data &node,
                                  widget::layout_result   &layout,
                                  float                    delta_seconds) -> bool
        {
            auto active = false;

            for (const auto &child : node.children)
                active |= apply_scroll_offsets(current, child, layout, delta_seconds);

            if (!node.scrollable) return active;

            const auto *bounds = layout.find(node.id);
            if (bounds == nullptr) return active;

            auto content = std::optional<math::rect> {};
            accumulate_content(node, layout, content);

            auto &offset = current.scrolls[node.id];
            auto &target = current.scroll_targets[node.id];

            if (!content)
            {
                offset = {};
                target = {};
                current.scroll_metrics.erase(node.id);
                return active;
            }

            const auto content_right  = bounds->right() - node.padding.right;
            const auto content_bottom = bounds->bottom() - node.padding.bottom;
            const auto max_x          = std::max(0.F, content->right() - content_right);
            const auto max_y          = std::max(0.F, content->bottom() - content_bottom);

            target.x = std::clamp(target.x, 0.F, max_x);
            target.y = std::clamp(target.y, 0.F, max_y);

            if (m_reduced_motion or delta_seconds <= 0.F) { offset = target; }
            else
            {
                const auto factor = 1.F - std::exp(-delta_seconds * 24.F);
                offset           += (target - offset) * factor;

                if (std::abs(target.x - offset.x) < 0.25F) offset.x = target.x;
                if (std::abs(target.y - offset.y) < 0.25F) offset.y = target.y;
            }

            offset.x = std::clamp(offset.x, 0.F, max_x);
            offset.y = std::clamp(offset.y, 0.F, max_y);

            current.scroll_metrics[node.id] = {
                offset,
                { max_x, max_y },
                { std::max(0.F, bounds->size.x - node.padding.left - node.padding.right),
                  std::max(0.F, bounds->size.y - node.padding.top - node.padding.bottom) },
            };

            if (offset.x != 0.F or offset.y != 0.F)
                translate_subtree(node, layout, { -offset.x, -offset.y });

            active |= offset.x != target.x or offset.y != target.y;
            return active;
        }


        void handle_wheel(session &current, const SDL_Event &native)
        {
            hide_tooltip(current);
            current.tooltip_hover.reset();

            const auto *hit   = hit_at(current, native.wheel.mouse_x, native.wheel.mouse_y);
            auto        value = make_event(event::kind::scroll, hit);
            const auto  point = to_logical(native.wheel.mouse_x, native.wheel.mouse_y);

            // Shift+wheel is the conventional horizontal scroll gesture on
            // mice without a horizontal wheel. Platforms that already report
            // a horizontal delta keep their native value.
            auto scroll_x = native.wheel.x;
            auto scroll_y = native.wheel.y;

            if (scroll_x == 0.F and (SDL_GetModState() & SDL_KMOD_SHIFT) != 0)
            {
                scroll_x = scroll_y;
                scroll_y = 0.F;
            }

            value.x         = point.x;
            value.y         = point.y;
            value.modifiers = static_cast<std::uint16_t>(SDL_GetModState());
            value.scroll_x  = scroll_x;
            value.scroll_y  = scroll_y;

            if (const auto *scroller = scroll_target(hit); scroller != nullptr)
            {
                const auto dx = -scroll_x * 48.F;
                const auto dy = -scroll_y * 48.F;

                auto &target = current.scroll_targets[scroller->id];
                target.x    += dx;
                target.y    += dy;

                // Move part of the way immediately so wheel input feels
                // responsive, then ease the remainder on later frames.
                auto &offset = current.scrolls[scroller->id];
                offset.x    += dx * 0.4F;
                offset.y    += dy * 0.4F;

                mark_layout_dirty(current);
            }

            dispatch(value, current);
        }


        [[nodiscard]]
        static auto containing_focus_group(const widget::node_data &node) noexcept
            -> const widget::node_data *
        {
            for (const auto *ancestor = node.parent; ancestor != nullptr;
                 ancestor             = ancestor->parent)
                if (ancestor->focus_group) return ancestor;

            return nullptr;
        }


        /// Moves focus between members of the nearest enclosing focus group.
        /// Controls that use arrow keys themselves are excluded, and modified
        /// arrows are left to shortcuts.
        auto handle_focus_group_key(session &current, const SDL_Event &native) -> bool
        {
            if (native.type != SDL_EVENT_KEY_DOWN) return false;
            if ((native.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) != 0) return false;

            auto *node = focused_node(current);
            if (node == nullptr or !_impl::effectively_visible(*node)) return false;

            switch (node->control)
            {
            case widget::control_kind::text_input:
            case widget::control_kind::slider:
            case widget::control_kind::combo_box:
            case widget::control_kind::combo_option:
            case widget::control_kind::list_item:
            case widget::control_kind::tree_item:          return false;
            case widget::control_kind::list_view:
            case widget::control_kind::tree_view:
            case widget::control_kind::combo_popup:
            case widget::control_kind::checkbox_indicator:
            case widget::control_kind::radio_indicator:
            case widget::control_kind::none:               break;
            }

            const auto *group = containing_focus_group(*node);
            if (group == nullptr) return false;

            auto delta = 0;
            auto edge  = false;
            auto last  = false;

            switch (native.key.key)
            {
            case SDLK_LEFT:
            case SDLK_UP:    delta = -1; break;
            case SDLK_RIGHT:
            case SDLK_DOWN:  delta = 1; break;
            case SDLK_HOME:  edge = true; break;
            case SDLK_END:
                edge = true;
                last = true;
                break;
            default: return false;
            }

            if (native.key.repeat and edge) return true;

            auto members = std::vector<const widget::node_data *> {};
            gather_group_members(*group, current.focus_groups, members);
            if (members.empty()) return false;

            auto       index = std::size_t { 0 };
            const auto found
                = std::find_if(members.begin(), members.end(), [&current](const auto *member)
                               { return current.focused and member->id == *current.focused; });
            if (found != members.end()) index = static_cast<std::size_t>(found - members.begin());

            if (edge)
                index = last ? members.size() - 1 : 0;
            else
            {
                const auto count = static_cast<std::ptrdiff_t>(members.size());
                const auto next  = (static_cast<std::ptrdiff_t>(index) + delta + count) % count;
                index            = static_cast<std::size_t>(next);
            }

            if (current.focused and members[index]->id == *current.focused) return true;
            focus_node(current, *members[index], true);
            return true;
        }


        struct view_shortcut_match
        {
            const widget::node_data *node    = nullptr;
            const shortcut_binding  *binding = nullptr;
            std::size_t              depth   = 0;
        };


        /// Finds the deepest matching shortcut in a visible subtree. Ties keep
        /// the first binding in document order.
        static void find_view_shortcut(const widget::node_data &node,
                                       std::uint32_t            key,
                                       std::uint16_t            modifiers,
                                       bool                     repeat,
                                       std::size_t              depth,
                                       bool                     hidden,
                                       view_shortcut_match     &best)
        {
            if (!node.visible) hidden = true;

            if (!hidden and !node.disabled)
                for (const auto &binding : node.shortcuts)
                {
                    if (binding.chord.key != key) continue;
                    if (shortcut_modifiers(binding.chord.modifiers) != modifiers) continue;
                    if (repeat and !binding.chord.repeat) continue;

                    if (best.node == nullptr or depth > best.depth)
                        best = { &node, &binding, depth };
                    break;
                }

            const auto children_hidden = hidden or !node.visible or node.disabled;
            for (const auto &child : node.children)
                find_view_shortcut(child, key, modifiers, repeat, depth + 1, children_hidden, best);
        }


        /// While a text field has focus, plain keys must reach the field instead
        /// of a shortcut. Function keys never insert text and still work.
        [[nodiscard]]
        auto shortcut_key_allowed(session      &current,
                                  std::uint32_t key,
                                  std::uint16_t modifiers) const -> bool
        {
            const auto *focused = focused_node(current);
            if (focused == nullptr or focused->control != widget::control_kind::text_input
                or !_impl::effectively_visible(*focused))
                return true;

            if ((modifiers & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) != 0) return true;

            return (key >= SDLK_F1 and key <= SDLK_F12) or (key >= SDLK_F13 and key <= SDLK_F24);
        }


        /// Dispatches a view-scoped shortcut when one matches, then falls back
        /// to application-wide bindings.
        auto handle_shortcut_key(session &current, const SDL_Event &native) -> bool
        {
            if (native.type != SDL_EVENT_KEY_DOWN) return false;
            if (m_shortcuts.empty() and !current.has_view_shortcuts) return false;

            const auto modifiers = shortcut_modifiers(native.key.mod);
            if (!shortcut_key_allowed(current, native.key.key, modifiers)) return false;

            auto best = view_shortcut_match {};
            if (current.has_view_shortcuts and current.tree)
            {
                const widget::node_data *scope = &*current.tree;
                if (current.active_modal) scope = find_node(current, *current.active_modal);

                if (scope != nullptr)
                    find_view_shortcut(*scope, native.key.key, modifiers, native.key.repeat, 0,
                                       false, best);
            }

            if (best.binding != nullptr)
            {
                auto value      = make_event(event::kind::key_down, best.node);
                value.key       = native.key.key;
                value.modifiers = modifiers;
                value.repeat    = native.key.repeat;
                value.action    = best.binding->action;
                dispatch(value, current);
                return true;
            }

            for (const auto &binding : m_shortcuts)
            {
                if (binding.chord.key != native.key.key) continue;
                if (shortcut_modifiers(binding.chord.modifiers) != modifiers) continue;
                if (native.key.repeat and !binding.chord.repeat) continue;

                auto value      = make_event(event::kind::key_down, nullptr);
                value.key       = native.key.key;
                value.modifiers = modifiers;
                value.repeat    = native.key.repeat;
                value.action    = binding.action;
                dispatch(value, current);
                return true;
            }

            return false;
        }


        void handle_key(session &current, render_context &context, const SDL_Event &native)
        {
            if (native.type == SDL_EVENT_KEY_DOWN)
            {
                hide_tooltip(current);
                current.tooltip_hover.reset();

                if (handle_focus_key(current, native)) return;

                // Escape first closes an IME composition or an open combo,
                // then cancels the active modal.
                if (native.key.key == SDLK_ESCAPE and !current.preedit_id
                    and current.open_combos.empty() and current.active_modal)
                {
                    if (current.tree)
                        if (auto *modal = current.tree->find(*current.active_modal);
                            modal != nullptr and !modal->cancel_action.empty())
                        {
                            auto value      = make_event(event::kind::key_down, modal);
                            value.key       = native.key.key;
                            value.modifiers = static_cast<std::uint16_t>(native.key.mod);
                            value.action    = modal->cancel_action;
                            dispatch(value, current);
                        }
                    return;
                }

                if (handle_control_key(current, context, native)) return;
                if (handle_shortcut_key(current, native)) return;
                if (handle_focus_group_key(current, native)) return;
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
            // Committing a composition replaces the preedit; it is never
            // inserted together with the committed text.
            current.preedit_id.reset();
            current.preedit_text.clear();
            current.preedit_caret = 0;

            if (native.text.text != nullptr)
            {
                auto *node = focused_node(current);
                if (node != nullptr and node->control == widget::control_kind::text_input
                    and _impl::effectively_visible(*node))
                {
                    const auto text = node->multiline
                                        ? widget::sanitize_multiline(native.text.text)
                                        : widget::sanitize_single_line(native.text.text);
                    if (!text.empty())
                    {
                        auto &edit  = current.edits[node->id];
                        edit.caret  = widget::clamp_offset(node->text, edit.caret);
                        edit.anchor = widget::clamp_offset(node->text, edit.anchor);
                        record_undo(current, *node, edit);
                        widget::insert_text(node->text, edit, text);
                        apply_text_edit(current, *node);
                        current.text_caret_goals.erase(node->id);
                    }
                    current.dirty = true;
                    return;
                }
            }

            auto value = make_event(event::kind::text_input, nullptr);
            if (native.text.text != nullptr) value.text = native.text.text;
            dispatch(value, current);
        }


        void handle_text_editing(session &current, const SDL_Event &native)
        {
            auto *node = focused_node(current);
            if (node == nullptr or node->control != widget::control_kind::text_input
                or !_impl::effectively_visible(*node) or native.edit.text == nullptr)
            {
                current.preedit_id.reset();
                current.preedit_text.clear();
                current.preedit_caret = 0;
                current.dirty         = true;
                return;
            }

            auto text = node->multiline ? widget::sanitize_multiline(native.edit.text)
                                        : widget::sanitize_single_line(native.edit.text);
            if (text.empty())
            {
                current.preedit_id.reset();
                current.preedit_text.clear();
                current.preedit_caret = 0;
            }
            else
            {
                current.preedit_id   = node->id;
                current.preedit_text = std::move(text);

                // SDL reports the selection inside the composition, or -1
                // when the platform does not provide one. With no selection,
                // keep the caret at the end of the composition.
                auto selection_end = current.preedit_text.size();
                if (native.edit.start >= 0)
                    selection_end = static_cast<std::size_t>(native.edit.start)
                                  + static_cast<std::size_t>(std::max(0, native.edit.length));

                current.preedit_caret = widget::clamp_offset(current.preedit_text, selection_end);
            }

            current.dirty = true;
        }


        void handle_resize(session &current, render_context &context)
        {
            sync_metrics(current, context);

            auto value   = make_event(event::kind::resize, nullptr);
            value.width  = m_width;
            value.height = m_height;
            dispatch(value, current);
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
                clear_pointer_state(current);
                update_cursor(current, nullptr, {});
                break;

            case SDL_EVENT_WINDOW_FOCUS_LOST:
                clear_pointer_state(current);
                close_all_combos(current);

                // The platform cancels any composition when focus leaves, so
                // do not keep showing a preedit that can no longer be
                // committed.
                current.preedit_id.reset();
                current.preedit_text.clear();
                current.preedit_caret = 0;
                current.dirty         = true;

                if (m_text_input_active)
                {
                    SDL_StopTextInput(m_window.get());
                    m_text_input_active = false;
                }
                break;

            case SDL_EVENT_WINDOW_FOCUS_GAINED: current.dirty = true; break;

            case SDL_EVENT_WINDOW_EXPOSED:
            case SDL_EVENT_WINDOW_RESTORED: current.dirty = true; break;

            case SDL_EVENT_WINDOW_RESIZED: handle_resize(current, context); break;

            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
            case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED: sync_metrics(current, context); break;

            case SDL_EVENT_MOUSE_MOTION: handle_pointer_move(current, context, native); break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: handle_pointer_button(current, context, native); break;

            case SDL_EVENT_MOUSE_WHEEL: handle_wheel(current, native); break;

            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:   handle_key(current, context, native); break;

            case SDL_EVENT_TEXT_INPUT: handle_text_input(current, native); break;

            case SDL_EVENT_TEXT_EDITING: handle_text_editing(current, native); break;

            default: break;
            }

            // Start or stop text input as soon as focus changes. Presenting also
            // synchronizes it, but a fast typist (or a synthetic key batch) can
            // deliver the next text event before the frame is presented.
            sync_text_input(current);
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

                mark_view_dirty(current);
            }

            return true;
        }


        void pump_subscription(session &current) const
        {
            if (!m_subscription) return;
            if (auto message = m_subscription())
            {
                current.messages.push_back(std::move(*message));
                mark_view_dirty(current);
            }
        }


        /// Advances eased interaction feedback and builds the progress map the
        /// renderer uses to blend hover, press, and focus states.
        /// \return \c true while at least one transition is still moving.
        auto
        advance_interactions(session                 &current,
                             const widget::node_data &root,
                             float                    delta_seconds,
                             std::unordered_map<widget::widget_id, interaction_progress> &output)
            -> bool
        {
            const auto *focused = current.focused ? find_node(current, *current.focused) : nullptr;
            const auto  show_focus
                = focused != nullptr
               && (current.focus_visible or focused->control == widget::control_kind::text_input);

            // With no transitions and no pointer or focus target there is
            // nothing to animate or prune. Skip walking the whole tree.
            if (current.interactions.empty() and !current.hovered and !current.pressed
                and !(show_focus and current.focused))
            {
                output.clear();
                return false;
            }

            // Focus is drawn only on the focused widget, never its ancestors.
            output = _impl::interaction_targets(root, current.hovered, current.pressed,
                                                current.focused, show_focus, &current.nodes);

            auto active = false;

            const auto visit = [&](const widget::node_data &node, auto &&self) -> void
            {
                if (node.motion.duration <= 0.F) { current.interactions.erase(node.id); }
                else
                {
                    auto &animation = current.interactions[node.id];
                    animation.spec  = node.motion;

                    const auto target
                        = output.contains(node.id) ? output[node.id] : interaction_progress {};

                    if (!animation.initialized)
                    {
                        animation.hover.snap(target.hover);
                        animation.press.snap(target.press);
                        animation.focus.snap(target.focus);
                        animation.initialized = true;
                    }

                    animation.hover.set_target(target.hover);
                    animation.press.set_target(target.press);
                    animation.focus.set_target(target.focus);

                    const auto duration = m_reduced_motion ? 0.F : animation.spec.duration;
                    active
                        |= animation.hover.advance(delta_seconds, duration, animation.spec.curve);
                    active
                        |= animation.press.advance(delta_seconds, duration, animation.spec.curve);
                    active
                        |= animation.focus.advance(delta_seconds, duration, animation.spec.curve);

                    output[node.id] = {
                        animation.hover.current(),
                        animation.press.current(),
                        animation.focus.current(),
                    };
                }

                for (const auto &child : node.children) self(child, self);
            };

            visit(root, visit);

            // Every live widget with a transition has an entry in `output`.
            // Checking that map is O(1), unlike searching the whole tree for
            // each stale animation.
            for (auto it = current.interactions.begin(); it != current.interactions.end();)
                if (!output.contains(it->first))
                    it = current.interactions.erase(it);
                else
                    ++it;

            return active;
        }


        /// Advances toast lifetimes and reports whether any remain visible.
        auto advance_toasts(float delta_seconds) -> bool
        {
            auto repaint = false;

            for (auto it = m_toasts.begin(); it != m_toasts.end();)
            {
                it->age += std::max(0.F, delta_seconds);

                if (it->age >= it->duration)
                {
                    it      = m_toasts.erase(it);
                    repaint = true;
                    continue;
                }

                const auto remaining = it->duration - it->age;
                if (it->age < toast_fade_in or remaining < toast_fade_out) repaint = true;

                ++it;
            }

            return repaint;
        }


        [[nodiscard]]
        auto build_toasts() const -> std::vector<toast_message>
        {
            auto result = std::vector<toast_message> {};
            result.reserve(m_toasts.size());

            for (const auto &toast : m_toasts)
            {
                auto opacity = 1.F;

                if (toast.age < toast_fade_in) opacity = toast.age / toast_fade_in;

                const auto remaining = toast.duration - toast.age;
                if (remaining < toast_fade_out)
                    opacity = std::min(opacity, remaining / toast_fade_out);

                auto offset = 0.F;
                if (!m_reduced_motion and toast.age < toast_fade_in)
                    offset = (1.F - (toast.age / toast_fade_in)) * 10.F;

                result.push_back({ toast.text, std::clamp(opacity, 0.F, 1.F), offset });
            }

            return result;
        }


        /// Rebuilds the tree from the view function and measures it. State
        /// changes, control edits, resizes, and theme changes take this path.
        template <typename Measure>
        auto rebuild(session &current, const Measure &measure, float delta_seconds) -> bool
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

            // The id index stores views into node ids, so the tree must be in
            // its final location before it is built. Keeping one index per
            // tree makes lookups constant-time instead of walking the tree.
            current.nodes.clear();
            current.tree = std::move(*next_tree);
            rebuild_node_index(current);

            sync_controls(current, *current.tree);
            apply_multi_selection(*current.tree);
            enforce_modal_focus(current, *current.tree);
            apply_autofocus(current, *current.tree);

            auto next_layout
                = calculate_layout(*current.tree, measure,
                                   { static_cast<float>(m_width), static_cast<float>(m_height) });
            if (!next_layout)
            {
                std::println(stderr, "sleet: layout failed: {}", next_layout.error().what());
                return false;
            }

            current.layout       = std::move(*next_layout);
            current.view_dirty   = false;
            current.layout_dirty = false;

            current.scroll_metrics.clear();
            ensure_visible(current, *current.layout);
            current.scroll_animating
                = apply_scroll_offsets(current, *current.tree, *current.layout, delta_seconds);

            return true;
        }


        /// Re-measures the existing tree. Used for smooth scrolling, combo
        /// visibility, and focus reveal, where no view state changed.
        template <typename Measure>
        auto reflow(session &current, const Measure &measure, float delta_seconds) -> bool
        {
            if (!current.tree) return true;

            auto next_layout
                = calculate_layout(*current.tree, measure,
                                   { static_cast<float>(m_width), static_cast<float>(m_height) });
            if (!next_layout)
            {
                std::println(stderr, "sleet: layout failed: {}", next_layout.error().what());
                return false;
            }

            current.layout       = std::move(*next_layout);
            current.layout_dirty = false;

            current.scroll_metrics.clear();
            ensure_visible(current, *current.layout);
            current.scroll_animating
                = apply_scroll_offsets(current, *current.tree, *current.layout, delta_seconds);

            return true;
        }


        /// Paints the current tree and layout. Render-only changes such as
        /// hover fades, tooltips, and toasts take this path without rebuilding
        /// the view or measuring text again.
        auto present(session &current, render_context &context, float delta_seconds) -> bool
        {
            if (!current.tree or !current.layout) return true;

            auto       progress = std::unordered_map<widget::widget_id, interaction_progress> {};
            const auto animating
                = advance_interactions(current, *current.tree, delta_seconds, progress);

            context.set_hovered(current.hovered);
            context.set_pressed(current.pressed);
            context.set_interaction_progress(std::move(progress));
            context.set_scroll_metrics(current.scroll_metrics);

            if (current.tooltip_visible)
                context.set_tooltip(current.tooltip_text, current.tooltip_anchor);
            else
                context.clear_tooltip();

            context.set_toasts(build_toasts());

            const auto *focused_node_ptr
                = current.focused ? find_node(current, *current.focused) : nullptr;

            // Keyboard focus rings are focus-visible; pointer focus on a text
            // input still shows its caret and border.
            const bool show_focus
                = focused_node_ptr != nullptr
              and (current.focus_visible
                   or focused_node_ptr->control == widget::control_kind::text_input);
            context.set_focused(show_focus ? current.focused : std::nullopt);

            if (focused_node_ptr != nullptr
                and focused_node_ptr->control == widget::control_kind::text_input)
            {
                auto scroll_y = 0.F;

                if (focused_node_ptr->multiline and current.layout)
                    if (const auto *bounds = current.layout->find(focused_node_ptr->id);
                        bounds != nullptr)
                        scroll_y = update_text_scroll(context, current, *focused_node_ptr, *bounds,
                                                      current.edits[focused_node_ptr->id].caret);

                context.set_text_selection(focused_node_ptr->id,
                                           current.edits[focused_node_ptr->id], scroll_y);

                if (current.preedit_id and *current.preedit_id == focused_node_ptr->id)
                    context.set_preedit(current.preedit_id, current.preedit_text,
                                        current.preedit_caret);
                else
                    context.set_preedit(std::nullopt, {}, 0);
            }
            else
            {
                context.set_text_selection(std::nullopt, {});
                context.set_preedit(std::nullopt, {}, 0);
            }

            // Focus changes can arrive through reflow as well as rebuild, so
            // the platform text-input state is synchronized here, where every
            // presented frame passes.
            sync_text_input(current);
            sync_text_input_area(current, context);

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

            current.dirty = animating or current.scroll_animating;
            return true;
        }


        auto run_impl() -> int
        {
            auto context = render_context { m_renderer.get(), m_theme };
            context.set_pixel_scale(pixel_scale());
            context.set_viewport({ static_cast<float>(m_width), static_cast<float>(m_height) });

            // Expose the live context to set_theme, and clear the pointer on
            // every exit path so it never dangles.
            struct context_binding
            {
                render_context *&slot;

                ~context_binding() { slot = nullptr; }
            } binding { m_context };

            m_context = &context;

            auto booted = m_boot();
            if (!booted)
            {
                std::println(stderr, "sleet: boot failed: {}", booted.error().what());
                return 1;
            }

            session current { std::move(booted->first) };
            current.messages.push_back(std::move(booted->second));

            const auto measure
                = [&context](std::string_view text, const widget::font_spec &font,
                             std::optional<float> width_limit, widget::text_wrap_mode wrap)
            { return context.measure_text(text, font, width_limit, wrap); };

            auto last_tick = SDL_GetTicks();

            while (true)
            {
                if (!pump_messages(current)) return 1;
                if (current.quit_requested or m_quit_requested) break;

                pump_subscription(current);

                // Themes can change font metrics, and toasts only need a
                // repaint. Re-measuring is cheap and keeps both correct.
                if (m_redraw_requested)
                {
                    m_redraw_requested = false;
                    mark_layout_dirty(current);
                }

                if (m_selection_requested)
                {
                    m_selection_requested = false;
                    mark_view_dirty(current);
                }

                const auto now     = SDL_GetTicks();
                const auto elapsed = static_cast<float>(now - last_tick) / 1000.F;
                last_tick          = now;
                const auto delta   = std::clamp(elapsed, 0.F, 0.1F);

                if (advance_toasts(elapsed)) current.dirty = true;

                if (current.view_dirty)
                {
                    if (!rebuild(current, measure, delta)) return 1;

                    // Layout is current, so the tooltip can follow its owner
                    // into this frame instead of lagging one behind.
                    update_tooltip(current);

                    if (!present(current, context, delta)) return 1;
                }
                else if (current.dirty)
                {
                    if ((current.layout_dirty or current.scroll_animating)
                        and !reflow(current, measure, delta))
                        return 1;

                    update_tooltip(current);

                    if (!present(current, context, delta)) return 1;
                }
                else
                {
                    // A tooltip delay can expire while the scene is idle.
                    update_tooltip(current);
                    if (current.dirty and !present(current, context, delta)) return 1;
                }

                if (current.dirty)
                {
                    // Presentation is paced by VSync while the scene is
                    // animating. Do not add an artificial wait here: doing so
                    // delays the next animated frame by a full refresh period
                    // in addition to the VSync block.
                    SDL_Event native;
                    while (SDL_PollEvent(&native)) handle_event(current, context, native);

                    // Without VSync, presentation returns immediately. Pace
                    // animated frames here so they do not spin the CPU.
                    if (!m_vsync_active) SDL_Delay(16);
                    continue;
                }

                // Idle: block until input arrives or a timer needs attention.
                // Subscriptions are polled, so they retain their previous
                // cadence. Tooltips wake exactly when their delay expires.
                auto timeout = idle_timeout(current);

                SDL_Event native;
                if (timeout == 0) continue;

                if (timeout < 0)
                {
                    if (!SDL_WaitEvent(&native)) continue;
                }
                else if (!SDL_WaitEventTimeout(&native, timeout))
                    continue;

                do { handle_event(current, context, native); } while (SDL_PollEvent(&native));
            }

            return 0;
        }
    };
}
