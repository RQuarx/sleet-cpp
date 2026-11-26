#pragma once
#include <algorithm>
#include <atomic>
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
#include <SDL3/SDL_timer.h>
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
                                        bool                                    show_focus)
            -> std::unordered_map<widget::widget_id, interaction_progress>
        {
            auto output = std::unordered_map<widget::widget_id, interaction_progress> {};

            if (hovered and root.find(*hovered) != nullptr) output[*hovered].hover = 1.F;
            if (pressed and root.find(*pressed) != nullptr) output[*pressed].press = 1.F;

            if (show_focus and focused and root.find(*focused) != nullptr)
                output[*focused].focus = 1.F;

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
        }

        ~application()
        {
            if (m_text_input_active) SDL_StopTextInput(m_window.get());
            if (m_current_cursor != nullptr) SDL_SetCursor(SDL_GetDefaultCursor());
            if (m_text_cursor != nullptr) SDL_DestroyCursor(m_text_cursor);
            if (m_pointer_cursor != nullptr) SDL_DestroyCursor(m_pointer_cursor);
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
        void set_reduced_motion(bool value = true) noexcept { m_reduced_motion = value; }


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

        /// Removes every active toast immediately.
        void clear_toasts()
        {
            if (m_toasts.empty()) return;
            m_toasts.clear();
            m_redraw_requested = true;
        }


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

        SDL_Cursor *m_text_cursor    = nullptr;
        SDL_Cursor *m_pointer_cursor = nullptr;
        SDL_Cursor *m_current_cursor = nullptr;

        /// Whether presentation blocks on the display refresh. Without it the
        /// loop paces animated frames itself.
        bool m_vsync_active = false;

        render_context *m_context = nullptr;

        /// Set by set_theme so the loop repaints even when no input arrives.
        std::atomic<bool> m_redraw_requested { false };

        /// Disables easing globally, for accessibility settings.
        bool m_reduced_motion = false;

        /// Whether SDL text input is currently enabled for the window.
        bool m_text_input_active = false;

        struct toast_entry
        {
            std::string text;
            float       age      = 0.F;
            float       duration = 3.F;
        };

        std::deque<toast_entry> m_toasts;

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
            std::optional<widget::widget_id>     hovered;
            std::optional<widget::widget_id>     pointer_target;
            std::optional<widget::widget_id>     pressed;
            std::optional<widget::widget_id>     focused;
            std::optional<widget::widget_id>     selecting;
            std::optional<widget::widget_id>     dragging;

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
            std::unordered_set<widget::widget_id>                         open_combos;

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
            const auto *focused
                = current.tree and current.focused ? current.tree->find(*current.focused) : nullptr;

            const auto should_enable = focused != nullptr
                                   and focused->control == widget::control_kind::text_input
                                   and _impl::effectively_visible(*focused);

            if (should_enable == m_text_input_active) return;

            if (should_enable)
                m_text_input_active = SDL_StartTextInput(m_window.get());
            else
            {
                SDL_StopTextInput(m_window.get());
                m_text_input_active = false;
            }
        }


        void update_cursor(const widget::node_data *hit)
        {
            auto *next = static_cast<SDL_Cursor *>(nullptr);

            if (const auto *control = _impl::control_target(hit);
                control != nullptr and control->control == widget::control_kind::text_input)
                next = m_text_cursor;
            else if (_impl::interactive_target(hit) != nullptr)
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
            if (!current.hovered and !current.pressed and !current.press_active
                and !current.selecting and !current.dragging and !current.pointer_target)
                return;

            current.hovered.reset();
            current.pointer_target.reset();
            current.pressed.reset();
            current.selecting.reset();
            current.dragging.reset();
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


        /// How long the pointer must rest before a tooltip appears.
        static constexpr auto tooltip_delay_ms = std::uint64_t { 550 };


        /// Restarts the rest timer after pointer movement. Moving within the
        /// same widget still counts as movement, so a tooltip only appears
        /// once the pointer settles.
        static void restart_tooltip(session &current)
        {
            hide_tooltip(current);
            current.tooltip_hover   = current.pointer_target;
            current.tooltip_started = SDL_GetTicks();
        }


        static void update_tooltip(session &current)
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

            const auto *node = current.tree->find(*current.pointer_target);
            if (node == nullptr)
            {
                // The hovered widget disappeared when the view was rebuilt.
                current.tooltip_hover.reset();
                hide_tooltip(current);
                return;
            }

            const widget::node_data *owner = nullptr;
            for (const auto *cursor = node; cursor != nullptr; cursor = cursor->parent)
                if (!cursor->tooltip_text.empty())
                {
                    owner = cursor;
                    break;
                }

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

            if (SDL_GetTicks() - current.tooltip_started < tooltip_delay_ms) return;

            current.tooltip_text    = owner->tooltip_text;
            current.tooltip_anchor  = *bounds;
            current.tooltip_visible = true;
            current.dirty           = true;
        }


        /// Returns the number of milliseconds the idle loop may sleep before a
        /// timer needs attention, or -1 to block until an SDL event arrives.
        [[nodiscard]]
        auto idle_timeout(const session &current) const -> Sint32
        {
            auto timeout = Sint32 { -1 };

            // Subscriptions are polled, so they keep the historical cadence.
            if (m_subscription) timeout = 16;

            if (current.pointer_target and !current.tooltip_visible)
            {
                const auto elapsed   = SDL_GetTicks() - current.tooltip_started;
                const auto remaining = elapsed >= tooltip_delay_ms
                                         ? Sint32 { 0 }
                                         : static_cast<Sint32>(tooltip_delay_ms - elapsed);
                timeout              = timeout < 0 ? remaining : std::min(timeout, remaining);
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


        static void collect_focusable(const widget::node_data                &node,
                                      std::vector<const widget::node_data *> &result)
        {
            if (!node.visible or node.disabled) return;
            if (node.focusable) result.push_back(&node);
            for (const auto &child : node.children) collect_focusable(child, result);
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
            collect_focusable(*scope, nodes);

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
                    auto *previous = root.find(*current.modal_return_focus);
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
                if (const auto *focused = root.find(*current.focused);
                    focused != nullptr and _impl::inside_subtree(*focused, *modal))
                    return;

            std::vector<const widget::node_data *> nodes;
            collect_focusable(*modal, nodes);

            if (nodes.empty())
            {
                current.focused.reset();
                current.selecting.reset();
            }
            else
                focus_node(current, *nodes.front());

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
                const auto *node = root.find(*it);
                if (node == nullptr or node->control != widget::control_kind::combo_box
                    or !_impl::effectively_visible(*node))
                    it = current.open_combos.erase(it);
                else
                    ++it;
            }

            apply_combo_visibility(root, current.open_combos);

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

            const auto is_text_input = [&root](const widget::widget_id &id)
            {
                const auto *node = root.find(id);
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
                const auto *node = root.find(it->first);
                if (node == nullptr or !node->scrollable)
                    it = current.scrolls.erase(it);
                else
                    ++it;
            }

            for (auto it = current.scroll_targets.begin(); it != current.scroll_targets.end();)
            {
                const auto *node = root.find(it->first);
                if (node == nullptr or !node->scrollable)
                    it = current.scroll_targets.erase(it);
                else
                    ++it;
            }

            if (current.focused)
            {
                const auto *node = root.find(*current.focused);
                if (node == nullptr or !node->focusable or !_impl::effectively_visible(*node))
                    current.focused.reset();
                else if (node->control == widget::control_kind::text_input
                         and !current.edits.contains(node->id))
                    current.edits[node->id].collapse(node->text.size());
            }

            if (current.selecting)
            {
                const auto *node = root.find(*current.selecting);
                if (node == nullptr or node->control != widget::control_kind::text_input
                    or !_impl::effectively_visible(*node))
                    current.selecting.reset();
            }

            if (current.hovered)
            {
                const auto *node = root.find(*current.hovered);
                if (node == nullptr or !_impl::effectively_visible(*node)) current.hovered.reset();
            }

            if (current.pressed)
            {
                const auto *node = root.find(*current.pressed);
                if (node == nullptr or !_impl::effectively_visible(*node))
                {
                    current.pressed.reset();
                    current.press_active = false;
                }
            }

            if (current.dragging)
            {
                const auto *node = root.find(*current.dragging);
                if (node == nullptr or node->control != widget::control_kind::slider
                    or !_impl::effectively_visible(*node))
                    current.dragging.reset();
            }

            if (current.preedit_id)
            {
                const auto *node = root.find(*current.preedit_id);
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
        auto caret_at(render_context          &context,
                      const widget::node_data &node,
                      const math::rect        &bounds,
                      float                    x) const -> std::size_t
        {
            const auto content = bounds.inset(node.padding);
            const auto target  = x - content.position.x;
            if (target <= 0.F) return 0;

            // Build the grapheme boundaries once, then binary-search the prefix
            // widths. This turns click-to-caret from O(n) text measurements
            // into O(log n).
            auto offsets = std::vector<std::size_t> { 0 };
            while (offsets.back() < node.text.size())
                offsets.push_back(widget::next_offset(node.text, offsets.back()));

            const auto width_at = [&](std::size_t index) -> std::optional<float>
            {
                auto width
                    = context.measure_text(std::string_view { node.text }.substr(0, offsets[index]),
                                           node.font, std::nullopt);
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


        /// Inserts the clipboard into \p node when it contains non-empty
        /// single-line text. Returns whether the text changed, so a no-op
        /// paste does not create a spurious undo step.
        auto paste_clipboard(session                &current,
                             widget::node_data      &node,
                             widget::text_selection &edit) const -> bool
        {
            auto *clipboard = SDL_GetClipboardText();
            if (clipboard == nullptr) return false;

            const auto text = widget::sanitize_single_line(clipboard);
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

            switch (native.key.key)
            {
            case SDLK_BACKSPACE:
                if (!edit.has_selection() and edit.caret == 0) return true;
                record_undo(current, node, edit);
                if (by_word ? widget::erase_word_backward(node.text, edit)
                            : widget::erase_backward(node.text, edit))
                    apply_text_edit(current, node);
                return true;

            case SDLK_DELETE:
                if (!edit.has_selection() and edit.caret >= node.text.size()) return true;
                record_undo(current, node, edit);
                if (by_word ? widget::erase_word_forward(node.text, edit)
                            : widget::erase_forward(node.text, edit))
                    apply_text_edit(current, node);
                return true;

            case SDLK_LEFT:
                widget::move_caret(node.text, edit,
                                   by_word ? widget::previous_word_offset(node.text, edit.caret)
                                           : widget::previous_offset(node.text, edit.caret),
                                   shift);
                current.dirty = true;
                return true;

            case SDLK_RIGHT:
                widget::move_caret(node.text, edit,
                                   by_word ? widget::next_word_offset(node.text, edit.caret)
                                           : widget::next_offset(node.text, edit.caret),
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
                if (!command) return false;
                widget::select_all(node.text, edit);
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
                }
                return true;

            case SDLK_V:
                if (!command) return false;
                (void)paste_clipboard(current, node, edit);
                return true;

            case SDLK_Z:
                if (!command) return false;
                restore_snapshot(current, node, edit, !shift);
                return true;

            case SDLK_Y:
                if (!command) return false;
                restore_snapshot(current, node, edit, false);
                return true;

            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                emit_control(current, event::kind::submit, node, node.text);
                return true;

            case SDLK_ESCAPE:
                // Escape cancels an active IME composition before it clears
                // focus; otherwise the field would lose focus mid-composition.
                if (current.preedit_id)
                {
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


        auto handle_control_key(session &current, const SDL_Event &native) -> bool
        {
            auto *node = focused_node(current);
            if (node == nullptr or !_impl::effectively_visible(*node)) return false;

            switch (node->control)
            {
            case widget::control_kind::text_input:
                return handle_text_input_key(current, *node, native);
            case widget::control_kind::slider:    return handle_slider_key(current, *node, native);
            case widget::control_kind::combo_box: return handle_combo_key(current, *node, native);
            case widget::control_kind::combo_option:
                return handle_combo_option_key(current, *node, native);
            case widget::control_kind::combo_popup:
            case widget::control_kind::checkbox_indicator:
            case widget::control_kind::radio_indicator:
            case widget::control_kind::none:               break;
            }

            if (node->action.empty()) return false;

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
                                  float                    x,
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
                        const auto scroll = text_scroll(context, *node, *bounds, edit.caret);
                        caret             = caret_at(context, *node, *bounds, x + scroll);
                    }

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
                update_slider(current, *node, x);
                break;

            case widget::control_kind::combo_box:
                focus_node(current, *node);
                set_combo_open(current, *node, !current.open_combos.contains(node->id));
                break;

            case widget::control_kind::combo_option: choose_combo_option(current, *node); break;

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

            update_cursor(hit);

            current.pointer_target
                = hit != nullptr ? std::optional<widget::widget_id> { hit->id } : std::nullopt;
            restart_tooltip(current);

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

            if (is_down)
            {
                hide_tooltip(current);
                current.tooltip_hover.reset();
            }

            update_cursor(hit);

            if (native.button.button == SDL_BUTTON_LEFT and is_down)
            {
                close_combos_outside(current, hit);
                update_focus_from_hit(current, hit);
                handle_control_press(current, context, hit, point.x,
                                     static_cast<int>(native.button.clicks));
            }

            auto value = make_event(is_down ? event::kind::mouse_down : event::kind::mouse_up, hit);
            value.x    = point.x;
            value.y    = point.y;
            value.button = native.button.button;

            if (native.button.button == SDL_BUTTON_LEFT and !is_down)
                if (!release_matches_press(current, hit)) value.action.clear();

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
            value.x           = point.x;
            value.y           = point.y;
            value.scroll_x    = native.wheel.x;
            value.scroll_y    = native.wheel.y;

            if (const auto *scroller = scroll_target(hit); scroller != nullptr)
            {
                const auto dx = -native.wheel.x * 48.F;
                const auto dy = -native.wheel.y * 48.F;

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


        void handle_key(session &current, const SDL_Event &native)
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
                    const auto text = widget::sanitize_single_line(native.text.text);
                    if (!text.empty())
                    {
                        auto &edit  = current.edits[node->id];
                        edit.caret  = widget::clamp_offset(node->text, edit.caret);
                        edit.anchor = widget::clamp_offset(node->text, edit.anchor);
                        record_undo(current, *node, edit);
                        widget::insert_text(node->text, edit, text);
                        apply_text_edit(current, *node);
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

            auto text = widget::sanitize_single_line(native.edit.text);
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
                update_cursor(nullptr);
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
            case SDL_EVENT_KEY_UP:   handle_key(current, native); break;

            case SDL_EVENT_TEXT_INPUT: handle_text_input(current, native); break;

            case SDL_EVENT_TEXT_EDITING: handle_text_editing(current, native); break;

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
            const auto *focused = current.focused ? root.find(*current.focused) : nullptr;
            const auto  show_focus
                = focused != nullptr
               && (current.focus_visible or focused->control == widget::control_kind::text_input);

            // Focus is drawn only on the focused widget, never its ancestors.
            output = _impl::interaction_targets(root, current.hovered, current.pressed,
                                                current.focused, show_focus);

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
            auto active = false;

            for (auto it = m_toasts.begin(); it != m_toasts.end();)
            {
                it->age += std::max(0.F, delta_seconds);

                if (it->age >= it->duration)
                {
                    it = m_toasts.erase(it);
                    continue;
                }

                ++it;
                active = true;
            }

            return active;
        }


        [[nodiscard]]
        auto build_toasts() const -> std::vector<toast_message>
        {
            constexpr auto fade_in  = 0.16F;
            constexpr auto fade_out = 0.30F;

            auto result = std::vector<toast_message> {};
            result.reserve(m_toasts.size());

            for (const auto &toast : m_toasts)
            {
                auto opacity = 1.F;

                if (toast.age < fade_in) opacity = toast.age / fade_in;

                const auto remaining = toast.duration - toast.age;
                if (remaining < fade_out) opacity = std::min(opacity, remaining / fade_out);

                auto offset = 0.F;
                if (!m_reduced_motion and toast.age < fade_in)
                    offset = (1.F - (toast.age / fade_in)) * 10.F;

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

            sync_controls(current, *next_tree);
            enforce_modal_focus(current, *next_tree);

            auto next_layout = calculate_layout(
                *next_tree, measure, { static_cast<float>(m_width), static_cast<float>(m_height) });
            if (!next_layout)
            {
                std::println(stderr, "sleet: layout failed: {}", next_layout.error().what());
                return false;
            }

            current.tree         = std::move(*next_tree);
            current.layout       = std::move(*next_layout);
            current.view_dirty   = false;
            current.layout_dirty = false;

            current.scroll_metrics.clear();
            ensure_visible(current, *current.layout);
            current.scroll_animating
                = apply_scroll_offsets(current, *current.tree, *current.layout, delta_seconds);
            sync_text_input(current);

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
                = current.focused ? current.tree->find(*current.focused) : nullptr;

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
                context.set_text_selection(focused_node_ptr->id,
                                           current.edits[focused_node_ptr->id]);

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

            const auto measure = [&context](std::string_view text, const widget::font_spec &font,
                                            std::optional<float> width_limit)
            { return context.measure_text(text, font, width_limit); };

            auto last_tick = SDL_GetTicks();

            while (true)
            {
                if (!pump_messages(current)) return 1;
                if (current.quit_requested) break;

                pump_subscription(current);

                // Themes can change font metrics, and toasts only need a
                // repaint. Re-measuring is cheap and keeps both correct.
                if (m_redraw_requested.exchange(false)) mark_layout_dirty(current);

                const auto now     = SDL_GetTicks();
                const auto elapsed = static_cast<float>(now - last_tick) / 1000.F;
                last_tick          = now;
                const auto delta   = std::clamp(elapsed, 0.F, 0.1F);

                if (advance_toasts(delta)) current.dirty = true;

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
