#pragma once
#include <SDL2/SDL.h>
#include <functional>
#include <vector>
#include <memory>
#include <unordered_set>
#include <unordered_map>

#include "engine/renderable_entity.h"

// Forward declarations
class EventHandlerEntry;

// Hash and equality for shared_ptr by pointer value
struct PtrHash {
    size_t operator()(const std::shared_ptr<EventHandlerEntry>& p) const noexcept {
        return std::hash<EventHandlerEntry*>()(p.get());
    }
};
struct PtrEqual {
    bool operator()(const std::shared_ptr<EventHandlerEntry>& a, const std::shared_ptr<EventHandlerEntry>& b) const noexcept {
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

    // Now returns void for the entry
    void register_entry(std::shared_ptr<EventHandlerEntry> entry);
    void register_entry(EventHandlerEntry* entry);
    std::shared_ptr<EventHandlerEntry> unregister_entry(std::shared_ptr<EventHandlerEntry> entry);

    bool handle_event(const SDL_Event& event);

private:
    EventHandler();
    ~EventHandler();

    static EventHandler* instance; // Singleton instance
    std::unordered_set<std::shared_ptr<EventHandlerEntry>, PtrHash, PtrEqual> m_entries;
};

// Base handler entry
class EventHandlerEntry {
public:
    using EventCallback = std::function<bool(const SDL_Event&)>;
    virtual ~EventHandlerEntry() = default;
    virtual bool matches(const SDL_Event& event) { return false; }
    void set_render_context(const RenderContext& context) {
        render_context = context;
    }
    // Legacy method for backward compatibility
    void set_window_id(unsigned int id) { 
        // Create a temporary context with just the window_id
        RenderContext ctx;
        ctx.window_id = id;
        render_context = ctx;
    }
    EventCallback callback;
    RenderContext render_context;
protected:
    EventHandlerEntry(RenderContext context = RenderContext(), EventCallback cb = nullptr)
        : callback(std::move(cb)), render_context(context) {}
    
    // Legacy constructor for backward compatibility
    EventHandlerEntry(unsigned int window_id, EventCallback cb = nullptr)
        : callback(std::move(cb)) {
        render_context.window_id = window_id;
    }
};

// Keyboard event handler entry with sticky key logic inside matches
class KeyboardEventHandlerEntry : public EventHandlerEntry {
public:
    KeyboardEventHandlerEntry(Uint32 type, SDL_Keycode key, EventCallback cb, bool sticky = false, RenderContext context = RenderContext());
    // Legacy constructor for backward compatibility
    KeyboardEventHandlerEntry(Uint32 type, SDL_Keycode key, EventCallback cb, bool sticky, unsigned int window_id);
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
    MouseEventHandlerEntry(int x, int y, int w, int h, EventCallback cb, RenderContext context = RenderContext())
        : EventHandlerEntry(context, std::move(cb)),
          rect_x(x), rect_y(y), rect_w(w), rect_h(h) {}
    
    // Legacy constructor for backward compatibility
    MouseEventHandlerEntry(int x, int y, int w, int h, EventCallback cb, unsigned int window_id)
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
    MouseClickEventHandlerEntry(Uint32 type, int x, int y, int w, int h, EventCallback cb, RenderContext context = RenderContext());
    // Legacy constructor for backward compatibility
    MouseClickEventHandlerEntry(Uint32 type, int x, int y, int w, int h, EventCallback cb, unsigned int window_id);
    bool matches(const SDL_Event& event) override;
private:
    Uint32 event_type;
    // rect_x, rect_y, rect_w, rect_h inherited
};

// Mouse motion handler entry for tracking cursor movement over a region
class MouseMotionEventHandlerEntry : public MouseEventHandlerEntry {
public:
    MouseMotionEventHandlerEntry(int x, int y, int w, int h, EventCallback cb, RenderContext context = RenderContext());
    // Legacy constructor for backward compatibility
    MouseMotionEventHandlerEntry(int x, int y, int w, int h, EventCallback cb, unsigned int window_id);
    bool matches(const SDL_Event& event) override;
    // rect_x, rect_y, rect_w, rect_h inherited
};

// Mouse enter/leave handler to detect when cursor enters or leaves a region
class MouseEnterLeaveEventHandlerEntry : public MouseEventHandlerEntry {
public:
    enum class Mode { ENTER, LEAVE };
    
    MouseEnterLeaveEventHandlerEntry(int x, int y, int w, int h, Mode mode, EventCallback cb, RenderContext context = RenderContext());
    // Legacy constructor for backward compatibility
    MouseEnterLeaveEventHandlerEntry(int x, int y, int w, int h, Mode mode, EventCallback cb, unsigned int window_id);
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
    GPIOEventHandlerEntry(int pin, int value, EventCallback cb, RenderContext context = RenderContext());
    // Legacy constructor for backward compatibility
    GPIOEventHandlerEntry(int pin, int value, EventCallback cb, unsigned int window_id);
    bool matches(const SDL_Event& event) override;
private:
    int gpio_pin;
    int gpio_value;
};
