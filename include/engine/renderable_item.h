#ifndef ENGINE_RENDERABLE_ITEM_H
#define ENGINE_RENDERABLE_ITEM_H

#include <SDL2/SDL.h> // For SDL_GetTicks
#include <cstdint>    // For standard integer types like Uint32
#include <string>

// Base interface for any system that participates in the event loop.
class IRenderableEntity {
public:
    virtual ~IRenderableEntity() {
        if (m_window) {
            if (m_context) {
                SDL_GL_DeleteContext(m_context);
            }
            SDL_DestroyWindow(m_window);
        }
    }
    
    virtual bool is_ready() = 0;
    virtual void render() = 0;
    virtual void present() = 0;

    virtual void activate_render_context() {
        if (m_window && m_context) {
            SDL_GL_MakeCurrent(m_window, m_context);
        }
    }

    // Function to calculate FPS for rendering
    virtual float get_render_fps() const { return m_render_fps; }

    // Function to calculate FPS for presentation
    virtual float get_present_fps() const { return m_present_fps; }

    // Initialize window and context
    virtual bool initialize_window(
        unsigned int width = 800, 
        unsigned int height = 600, 
        const std::string& title = "Window", 
        bool hidden = false
    ) {
        // Create window with appropriate flags
        Uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
        if (hidden) {
            window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN;
        }

        m_window = SDL_CreateWindow(
            title.c_str(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height,
            window_flags
        );
        
        if (!m_window) {
            SDL_Log("Failed to create window: %s", SDL_GetError());
            return false;
        }
        
        // Create OpenGL context
        m_context = SDL_GL_CreateContext(m_window);
        if (!m_context) {
            SDL_Log("Failed to create OpenGL context: %s", SDL_GetError());
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
            return false;
        }

        return true;
    }
    
    // Getters for window and context
    SDL_Window* get_window() const { return m_window; }
    SDL_GLContext get_context() const { return m_context; }
    Uint32 get_window_id() const { return m_window ? SDL_GetWindowID(m_window) : 0; }

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

    // SDL window/context management
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_context = nullptr;

private:
    float m_render_fps{0.0f};
    float m_present_fps{0.0f};
    Uint32 m_last_render_time{0};
    Uint32 m_last_present_time{0};
    int m_render_frame_count{0};
    int m_present_frame_count{0};
};

#endif // ENGINE_RENDERABLE_ITEM_H