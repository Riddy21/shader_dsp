#pragma once
#include <SDL2/SDL.h>

#include "engine/event_handler.h"
#include "engine/renderable_item.h"

class GraphicsComponent {
public:
    GraphicsComponent(
        const float x = 0.0f, 
        const float y = 0.0f, 
        const float width = 0.0f, 
        const float height = 0.0f
    ) : m_x(x), m_y(y), m_width(width), m_height(height) {}
    virtual ~GraphicsComponent() = default;
    virtual void render() = 0;

    virtual void register_event_handlers(EventHandler* event_handler) {}
    virtual void unregister_event_handlers() {}

    void set_position(const float x, const float y) {
        m_x = x;
        m_y = y;
    }

    void set_display_id(unsigned int id) {
        m_window_id = id;
    }
    
protected:
    float m_x = 0.0f;
    float m_y = 0.0f;
    float m_width = 0.0f;
    float m_height = 0.0f;
    unsigned int m_window_id = 0; // Window ID for this component

    // Event handler entries
    EventHandler* m_event_handler = nullptr;
    std::vector<EventHandlerEntry*> m_event_handler_entries;

    bool m_event_handlers_registered = false; // Flag to prevent double registration
};
