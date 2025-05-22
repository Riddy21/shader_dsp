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
class MouseEventHandlerEntry;
class MouseClickEventHandlerEntry;
class MouseMotionEventHandlerEntry;
class MouseEnterLeaveEventHandlerEntry;
class GlobalMouseUpEventHandlerEntry;
class GPIOEventHandlerEntry;

class EventHandler {
public:
    EventHandler();
    ~EventHandler();

    void register_entry(EventHandlerEntry* entry);
    bool unregister_entry(EventHandlerEntry* entry);

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
    IRenderableEntity* render_context; // TODO: Consider encapsulating this in another class
protected:
    EventHandlerEntry(IRenderableEntity* render_context_item, EventCallback cb)
        : render_context(render_context_item), callback(std::move(cb)) {}
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

// --- Mouse event handler base class ---
class MouseEventHandlerEntry : public EventHandlerEntry {
public:
    MouseEventHandlerEntry(IRenderableEntity* render_context_item, IRenderableEntity* display_context_item, Uint32 type, int x, int y, int w, int h, EventCallback cb)
        : EventHandlerEntry(render_context_item, std::move(cb)), event_type(type), rect_x(x), rect_y(y), rect_w(w), rect_h(h), display_context(display_context_item) {}
    
    // Constructor that uses render_context as display_context (for backward compatibility)
    MouseEventHandlerEntry(IRenderableEntity* render_context_item, Uint32 type, int x, int y, int w, int h, EventCallback cb)
        : EventHandlerEntry(render_context_item, std::move(cb)), event_type(type), rect_x(x), rect_y(y), rect_w(w), rect_h(h), display_context(render_context_item) {}
    
    virtual ~MouseEventHandlerEntry() = default;
    // No matches() here, subclasses must implement
protected:
    Uint32 event_type;
    int rect_x, rect_y, rect_w, rect_h;
    IRenderableEntity* display_context;
};

// Mouse click event handler entry
class MouseClickEventHandlerEntry : public MouseEventHandlerEntry {
public:
    MouseClickEventHandlerEntry(IRenderableEntity* render_context_item, IRenderableEntity* display_context_item, Uint32 type, int x, int y, int w, int h, EventCallback cb);
    bool matches(const SDL_Event& event) override;
};

// Mouse motion handler entry for tracking cursor movement over a region
class MouseMotionEventHandlerEntry : public MouseEventHandlerEntry {
public:
    MouseMotionEventHandlerEntry(IRenderableEntity* render_context_item, IRenderableEntity* display_context_item, Uint32 type, int x, int y, int w, int h, EventCallback cb);
    bool matches(const SDL_Event& event) override;
};

// Mouse enter/leave handler to detect when cursor enters or leaves a region
class MouseEnterLeaveEventHandlerEntry : public MouseEventHandlerEntry {
public:
    enum class Mode { ENTER, LEAVE };
    MouseEnterLeaveEventHandlerEntry(IRenderableEntity* render_context_item, IRenderableEntity* display_context_item, Uint32 type, int x, int y, int w, int h, Mode mode, EventCallback cb);
    bool matches(const SDL_Event& event) override;
private:
    // Helper methods for state tracking
    bool is_inside(int mouse_x, int mouse_y) const;
    void update_last_position(int mouse_x, int mouse_y);

    Mode mode;
    mutable bool was_inside = false;
    mutable int last_x = -1, last_y = -1;
};

// GPIO event handler entry (stub)
// TODO: To implement later
class GPIOEventHandlerEntry : public EventHandlerEntry {
public:
    GPIOEventHandlerEntry(IRenderableEntity* render_context_item, int pin, int value, EventCallback cb);
    bool matches(const SDL_Event& event) override;
private:
    int gpio_pin;
    int gpio_value;
};
