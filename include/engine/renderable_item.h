#ifndef ENGINE_RENDERABLE_ITEM_H
#define ENGINE_RENDERABLE_ITEM_H

#include <SDL2/SDL.h> // For SDL_GetTicks
#include <cstdint>    // For standard integer types like Uint32

// Base interface for any system that participates in the event loop.
class IRenderableEntity {
public:
    virtual ~IRenderableEntity() {}
    virtual bool is_ready() = 0;
    virtual void render() = 0;
    virtual void present() = 0;

    virtual void activate_render_context() = 0;

    // Function to calculate FPS for rendering
    virtual float get_render_fps() const { return m_render_fps; }

    // Function to calculate FPS for presentation
    virtual float get_present_fps() const { return m_present_fps; }

protected:
    void update_render_fps() {
        auto now = SDL_GetTicks();
        if (m_last_render_time > 0) {
            m_render_frame_count++;
            if (now - m_last_render_time >= 1000) {
                m_render_fps = m_render_frame_count * 1000.0f / (now - m_last_render_time);
                m_render_frame_count = 0;
                m_last_render_time = now;
            }
        } else {
            m_last_render_time = now;
        }
    }

    void update_present_fps() {
        auto now = SDL_GetTicks();
        if (m_last_present_time > 0) {
            m_present_frame_count++;
            if (now - m_last_present_time >= 1000) {
                m_present_fps = m_present_frame_count * 1000.0f / (now - m_last_present_time);
                m_present_frame_count = 0;
                m_last_present_time = now;
            }
        } else {
            m_last_present_time = now;
        }
    }

private:
    float m_render_fps{0.0f};
    float m_present_fps{0.0f};
    Uint32 m_last_render_time{0};
    Uint32 m_last_present_time{0};
    int m_render_frame_count{0};
    int m_present_frame_count{0};
};

#endif // ENGINE_RENDERABLE_ITEM_H