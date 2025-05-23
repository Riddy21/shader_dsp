#pragma once
#include <SDL2/SDL.h>

#include "engine/event_handler.h"
#include "engine/renderable_item.h"

class GraphicsComponent {
public:
    GraphicsComponent(const float x = 0.0f, const float y = 0.0f, const float width = 0.0f, const float height = 0.0f, 
                    IRenderableEntity* render_context = nullptr, IRenderableEntity* display_context = nullptr)
        : m_x(x), m_y(y), m_width(width), m_height(height), m_render_context(render_context), m_display_context(display_context){}
    virtual ~GraphicsComponent() = default;
    virtual void render() = 0;

    virtual void register_event_handlers(EventHandler* event_handler) = 0;
    virtual void unregister_event_handlers() = 0;

    void set_position(const float x, const float y) {
        m_x = x;
        m_y = y;
    }
    
    void set_render_context(IRenderableEntity* render_context) {
        m_render_context = render_context;
    }
    
    IRenderableEntity* get_render_context() const {
        return m_render_context;
    }

protected:
    float m_x = 0.0f;
    float m_y = 0.0f;
    float m_width = 0.0f;
    float m_height = 0.0f;
    IRenderableEntity* m_render_context = nullptr;
    IRenderableEntity* m_display_context = nullptr;
};
