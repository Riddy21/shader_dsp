#pragma once
#include <SDL2/SDL.h>
#include <functional>
#include <vector>
#include <memory>
#include <unordered_set>
#include <unordered_map>

#include "engine/renderable_item.h"

// Forward declarations
class EventHandlerEntry;

// Hash and equality functors for unique_ptr<EventHandlerEntry>
struct EntryPtrHash {
    std::size_t operator()(const std::unique_ptr<EventHandlerEntry>& ptr) const noexcept {
        return std::hash<EventHandlerEntry*>()(ptr.get());
    }
};
struct EntryPtrEqual {
    bool operator()(const std::unique_ptr<EventHandlerEntry>& a, const std::unique_ptr<EventHandlerEntry>& b) const noexcept {
        return a.get() == b.get();
    }
};

class EventHandler {
public:
    static EventHandler& get_instance() {
        if (!instance) {
            instance = new EventHandler();
        }
        return *instance;
    }

    EventHandler(const EventHandler&) = delete;
    EventHandler& operator=(const EventHandler&) = delete;

    void register_entry(EventHandlerEntry* entry);
    bool unregister_entry(EventHandlerEntry* entry);

    bool handle_event(const SDL_Event& event);

private:
    EventHandler();
    ~EventHandler();

    static EventHandler* instance; // Singleton instance
    std::unordered_map<EventHandlerEntry*, std::unique_ptr<EventHandlerEntry>> m_entries;
};

// Base handler entry
class EventHandlerEntry {
public:
    using EventCallback = std::function<bool(const SDL_Event&)>;
    virtual ~EventHandlerEntry() = default;
    virtual bool matches(const SDL_Event& event) { return false; }
    EventCallback callback;
    unsigned int window_id;
protected:
    EventHandlerEntry(unsigned int window_id = 0, EventCallback cb = nullptr)
        : callback(std::move(cb)), window_id(window_id) {}
};

// Keyboard event handler entry with sticky key logic inside matches
class KeyboardEventHandlerEntry : public EventHandlerEntry {
public:
    KeyboardEventHandlerEntry(Uint32 type, SDL_Keycode key, EventCallback cb, bool sticky = false, unsigned int window_id = 0);
    ~KeyboardEventHandlerEntry() = default;
    bool matches(const SDL_Event& event) override;
private:
    Uint32 event_type;
    SDL_Keycode keycode;
    bool sticky_keys;
    std::unordered_set<SDL_Keycode> pressed_keys;
};

// Mouse event handler entry base class (abstract)
class MouseEventHandlerEntry : public EventHandlerEntry {
protected:
    MouseEventHandlerEntry(int x, int y, int w, int h, EventCallback cb, unsigned int window_id = 0)
        : EventHandlerEntry(window_id, std::move(cb)),
          rect_x(x), rect_y(y), rect_w(w), rect_h(h) {}
    int rect_x, rect_y, rect_w, rect_h;
private:
    // Prevent direct instantiation
    MouseEventHandlerEntry(const MouseEventHandlerEntry&) = delete;
    MouseEventHandlerEntry& operator=(const MouseEventHandlerEntry&) = delete;
};

// Mouse click event handler entry
class MouseClickEventHandlerEntry : public MouseEventHandlerEntry {
public:
    MouseClickEventHandlerEntry(Uint32 type, int x, int y, int w, int h, EventCallback cb, unsigned int window_id = 0);
    bool matches(const SDL_Event& event) override;
private:
    Uint32 event_type;
    // rect_x, rect_y, rect_w, rect_h inherited
};

// Mouse motion handler entry for tracking cursor movement over a region
class MouseMotionEventHandlerEntry : public MouseEventHandlerEntry {
public:
    MouseMotionEventHandlerEntry(int x, int y, int w, int h, EventCallback cb, unsigned int window_id = 0);
    bool matches(const SDL_Event& event) override;
    // rect_x, rect_y, rect_w, rect_h inherited
};

// Mouse enter/leave handler to detect when cursor enters or leaves a region
class MouseEnterLeaveEventHandlerEntry : public MouseEventHandlerEntry {
public:
    enum class Mode { ENTER, LEAVE };
    
    MouseEnterLeaveEventHandlerEntry(int x, int y, int w, int h, Mode mode, EventCallback cb, unsigned int window_id = 0);
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
    GPIOEventHandlerEntry(int pin, int value, EventCallback cb, unsigned int window_id = 0);
    bool matches(const SDL_Event& event) override;
private:
    int gpio_pin;
    int gpio_value;
};
