#pragma once
#include <SDL2/SDL.h>
#include <functional>
#include <vector>
#include <memory>
#include <unordered_set>

#include "engine/renderable_item.h"

// Forward declarations
class EventHandlerEntry;
class KeyboardEventHandlerEntry;
class MouseClickEventHandlerEntry;
class GPIOEventHandlerEntry;

class EventHandler {
public:
    EventHandler();
    ~EventHandler();

    void register_entry(EventHandlerEntry * entry);

    bool handle_event(const SDL_Event& event);

private:
    std::vector<std::unique_ptr<EventHandlerEntry>> m_entries;
};

// Base handler entry
class EventHandlerEntry {
public:
    using EventCallback = std::function<bool(const SDL_Event&)>;
    virtual ~EventHandlerEntry() = default;
    virtual bool matches(const SDL_Event& event) = 0;
    EventCallback callback;
    IRenderableEntity* render_context_item; // TODO: Consider encapsulating this in another class
protected:
    EventHandlerEntry(IRenderableEntity* render_context_item, EventCallback cb)
        : render_context_item(render_context_item), callback(std::move(cb)) {}
};

// Keyboard event handler entry with sticky key logic inside matches
class KeyboardEventHandlerEntry : public EventHandlerEntry {
public:
    KeyboardEventHandlerEntry(IRenderableEntity* render_context_item, Uint32 type, SDL_Keycode key, EventCallback cb, bool sticky = false);
    ~KeyboardEventHandlerEntry() = default;
    bool matches(const SDL_Event& event) override;
private:
    Uint32 event_type;
    SDL_Keycode keycode;
    bool sticky_keys;
    std::unordered_set<SDL_Keycode> pressed_keys;
};

// Mouse event handler entry
class MouseClickEventHandlerEntry : public EventHandlerEntry {
public:
    MouseClickEventHandlerEntry(IRenderableEntity* render_context_item, Uint32 type, int x, int y, int w, int h, EventCallback cb);
    bool matches(const SDL_Event& event) override;
private:
    Uint32 event_type;
    int rect_x, rect_y, rect_w, rect_h;
};

// GPIO event handler entry (stub)
class GPIOEventHandlerEntry : public EventHandlerEntry {
public:
    GPIOEventHandlerEntry(IRenderableEntity* render_context_item, int pin, int value, EventCallback cb);
    bool matches(const SDL_Event& event) override;
private:
    int gpio_pin;
    int gpio_value;
};
