#pragma once
#include <SDL2/SDL.h>

class GraphicsComponent {
public:
    GraphicsComponent(const float x = 0.0f, const float y = 0.0f, const float width = 0.0f, const float height = 0.0f)
        : m_x(x), m_y(y), m_width(width), m_height(height) {}
    virtual ~GraphicsComponent() = default;
    virtual void render() = 0;
    virtual bool handle_event(const SDL_Event&) { return false;}
    void set_position(const float x, const float y) {
        m_x = x;
        m_y = y;
    }

protected:
    float m_x = 0.0f;
    float m_y = 0.0f;
    float m_width = 0.0f;
    float m_height = 0.0f;
};
