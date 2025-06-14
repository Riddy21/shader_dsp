#include <unordered_set>
#include <algorithm>

#include "engine/event_handler.h"
#include "engine/event_loop.h"

// Define EventCallback if not already defined
using EventCallback = std::function<bool(const SDL_Event&)>;

EventHandler* EventHandler::instance = nullptr;

EventHandler::EventHandler() {
    auto & event_loop = EventLoop::get_instance();
    event_loop.add_event_handler(this); // Register this event handler instance with the event loop
}
EventHandler::~EventHandler() {}

void EventHandler::register_entry(std::shared_ptr<EventHandlerEntry> entry) {
    m_entries.insert(entry);
}

void EventHandler::register_entry(EventHandlerEntry* entry) {
    if (entry) {
        auto shared_entry = std::shared_ptr<EventHandlerEntry>(entry);
        m_entries.insert(shared_entry);
    }
}

std::shared_ptr<EventHandlerEntry> EventHandler::unregister_entry(std::shared_ptr<EventHandlerEntry> entry) {
    auto it = m_entries.find(entry);
    if (it != m_entries.end()) {
        auto removed = *it;
        m_entries.erase(it);
        return removed;
    }
    return nullptr;
}

bool EventHandler::handle_event(const SDL_Event& event) {
    bool handled = false;
    std::vector<std::pair<EventCallback, RenderContext>> callbacks_and_contexts;

    for (const auto& entry : m_entries) {
        if (entry->matches(event)) {
            callbacks_and_contexts.emplace_back(entry->callback, entry->render_context);
        }
    }

    for (const auto& [callback, context] : callbacks_and_contexts) {
        context.activate(); // Activate the context before calling the callback
        handled |= callback(event);
        context.unactivate(); // Unactivate the context after calling the callback
    }
    return handled;
}

// --- KeyboardEventHandlerEntry ---

KeyboardEventHandlerEntry::KeyboardEventHandlerEntry(
    Uint32 type, SDL_Keycode key, EventCallback cb, bool sticky, RenderContext context)
    : EventHandlerEntry(context, std::move(cb)),
      event_type(type), keycode(key), sticky_keys(sticky) {}

KeyboardEventHandlerEntry::KeyboardEventHandlerEntry(
    Uint32 type, SDL_Keycode key, EventCallback cb, bool sticky, unsigned int window_id)
    : EventHandlerEntry(window_id, std::move(cb)),
      event_type(type), keycode(key), sticky_keys(sticky) {}

bool KeyboardEventHandlerEntry::matches(const SDL_Event& event) {
    if (render_context.window_id && event.key.windowID != render_context.window_id) return false;
    if (event.type == SDL_KEYUP) {
        pressed_keys.erase(event.key.keysym.sym);
    }
    if (event.type != event_type || event.key.keysym.sym != keycode) {
        return false;
    }
    if (event.type == SDL_KEYDOWN) {
        if (!sticky_keys && pressed_keys.count(keycode)) {
            return false;
        }
        pressed_keys.insert(keycode);
    }
    return true;
}

// --- MouseClickEventHandlerEntry ---

MouseClickEventHandlerEntry::MouseClickEventHandlerEntry(
    Uint32 type, int x, int y, int w, int h, EventCallback cb, RenderContext context)
    : MouseEventHandlerEntry(x, y, w, h, std::move(cb), context), event_type(type) {}

MouseClickEventHandlerEntry::MouseClickEventHandlerEntry(
    Uint32 type, int x, int y, int w, int h, EventCallback cb, unsigned int window_id)
    : MouseEventHandlerEntry(x, y, w, h, std::move(cb), window_id), event_type(type) {}

bool MouseClickEventHandlerEntry::matches(const SDL_Event& event) {
    if (render_context.window_id && event.button.windowID != render_context.window_id) return false;
    if (event.type != event_type) return false;
    int ex = event.button.x;
    int ey = event.button.y;
    return ex >= rect_x && ex < (rect_x + rect_w) && ey >= rect_y && ey < (rect_y + rect_h);
}

// --- MouseMotionEventHandlerEntry ---

MouseMotionEventHandlerEntry::MouseMotionEventHandlerEntry(
    int x, int y, int w, int h, EventCallback cb, RenderContext context)
    : MouseEventHandlerEntry(x, y, w, h, std::move(cb), context) {}

MouseMotionEventHandlerEntry::MouseMotionEventHandlerEntry(
    int x, int y, int w, int h, EventCallback cb, unsigned int window_id)
    : MouseEventHandlerEntry(x, y, w, h, std::move(cb), window_id) {}

bool MouseMotionEventHandlerEntry::matches(const SDL_Event& event) {
    if (render_context.window_id && event.motion.windowID != render_context.window_id) return false;
    if (event.type != SDL_MOUSEMOTION) return false;
    int ex = event.motion.x;
    int ey = event.motion.y;
    return ex >= rect_x && ex < (rect_x + rect_w) && ey >= rect_y && ey < (rect_y + rect_h);
}

// --- MouseEnterLeaveEventHandlerEntry ---

MouseEnterLeaveEventHandlerEntry::MouseEnterLeaveEventHandlerEntry(
    int x, int y, int w, int h, Mode mode, EventCallback cb, RenderContext context)
    : MouseEventHandlerEntry(x, y, w, h, std::move(cb), context), mode(mode) {}

MouseEnterLeaveEventHandlerEntry::MouseEnterLeaveEventHandlerEntry(
    int x, int y, int w, int h, Mode mode, EventCallback cb, unsigned int window_id)
    : MouseEventHandlerEntry(x, y, w, h, std::move(cb), window_id), mode(mode) {}

bool MouseEnterLeaveEventHandlerEntry::is_inside(int mouse_x, int mouse_y) const {
    return mouse_x >= rect_x && mouse_x < (rect_x + rect_w) && 
           mouse_y >= rect_y && mouse_y < (rect_y + rect_h);
}

void MouseEnterLeaveEventHandlerEntry::update_last_position(int mouse_x, int mouse_y) {
    last_x = mouse_x;
    last_y = mouse_y;
    was_inside = is_inside(mouse_x, mouse_y);
}

bool MouseEnterLeaveEventHandlerEntry::matches(const SDL_Event& event) {
    if (render_context.window_id && event.motion.windowID != render_context.window_id) return false;
    if (event.type != SDL_MOUSEMOTION) return false;
    int ex = event.motion.x;
    int ey = event.motion.y;
    bool is_now_inside = is_inside(ex, ey);
    if (last_x == -1 || last_y == -1) {
        update_last_position(ex, ey);
        return false;
    }
    if (mode == Mode::ENTER && !was_inside && is_now_inside) {
        update_last_position(ex, ey);
        return true;
    }
    if (mode == Mode::LEAVE && was_inside && !is_now_inside) {
        update_last_position(ex, ey);
        return true;
    }
    update_last_position(ex, ey);
    return false;
}

// --- GPIOEventHandlerEntry ---

GPIOEventHandlerEntry::GPIOEventHandlerEntry(
    int pin, int value, EventCallback cb, RenderContext context)
    : EventHandlerEntry(context, std::move(cb)) {
    gpio_pin = pin;
    gpio_value = value;
}

GPIOEventHandlerEntry::GPIOEventHandlerEntry(
    int pin, int value, EventCallback cb, unsigned int window_id)
    : EventHandlerEntry(window_id, std::move(cb)) {
    gpio_pin = pin;
    gpio_value = value;
}

bool GPIOEventHandlerEntry::matches(const SDL_Event& event) {
    // Implement GPIO event matching if you have custom SDL event types for GPIO
    return false;
}
