#include <unordered_set>

#include "engine/event_handler.h"
#include "engine/event_loop.h"

EventHandler::EventHandler() {}
EventHandler::~EventHandler() {}

void EventHandler::register_entry(EventHandlerEntry* entry) {
    m_entries.push_back(std::unique_ptr<EventHandlerEntry>(entry));
}

bool EventHandler::handle_event(const SDL_Event& event) {
    bool handled = false;
    for (auto& entry : m_entries) {
        if (entry->matches(event)) {
            entry->render_context_item->activate_render_context();
            handled |= entry->callback(event);
        }
    }
    return handled;
}

// --- KeyboardEventHandlerEntry ---

KeyboardEventHandlerEntry::KeyboardEventHandlerEntry(
    IRenderableEntity* render_context_item, Uint32 type, SDL_Keycode key, EventCallback cb, bool sticky)
    : EventHandlerEntry(render_context_item, std::move(cb)),
      event_type(type), keycode(key), sticky_keys(sticky) {}

bool KeyboardEventHandlerEntry::matches(const SDL_Event& event) {
    if (event.type == SDL_KEYUP) {
        pressed_keys.erase(keycode);
    }
    if (event.type != event_type || event.key.keysym.sym != keycode)
        return false;
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
    IRenderableEntity* render_context_item, Uint32 type, int x, int y, int w, int h, EventCallback cb)
    : EventHandlerEntry(render_context_item, std::move(cb)) {
    event_type = type;
    rect_x = x; rect_y = y; rect_w = w; rect_h = h;
}

bool MouseClickEventHandlerEntry::matches(const SDL_Event& event) {
    if (event.type != event_type) return false;
    int ex = event.button.x;
    int ey = event.button.y;
    return ex >= rect_x && ex < (rect_x + rect_w) && ey >= rect_y && ey < (rect_y + rect_h);
}

// --- GPIOEventHandlerEntry ---

GPIOEventHandlerEntry::GPIOEventHandlerEntry(
    IRenderableEntity* render_context_item, int pin, int value, EventCallback cb)
    : EventHandlerEntry(render_context_item, std::move(cb)) {
    gpio_pin = pin;
    gpio_value = value;
}

bool GPIOEventHandlerEntry::matches(const SDL_Event& event) {
    // Implement GPIO event matching if you have custom SDL event types for GPIO
    return false;
}
