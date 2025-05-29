#ifndef ENGINE_RENDERABLE_ITEM_H
#define ENGINE_RENDERABLE_ITEM_H

#include <SDL2/SDL.h> // For SDL_GetTicks
#include <cstdint>    // For standard integer types like Uint32
#include <string>

// Structure to encapsulate rendering context information
struct RenderContext {
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    unsigned int window_id = 0;
    std::string title;
    bool visible = true;
    
    // Default constructor
    RenderContext() = default;
    
    // Constructor with parameters
    RenderContext(SDL_Window* win, SDL_GLContext ctx, const std::string& win_title = "OpenGL Window", bool is_visible = true)
        : window(win), gl_context(ctx), title(win_title), visible(is_visible) 
    {
        if (window) {
            window_id = SDL_GetWindowID(window);
        }
    }
    
    // Activate this render context
    void activate() const {
        if (window && gl_context && SDL_GL_GetCurrentContext() != gl_context) {
            SDL_GL_MakeCurrent(window, gl_context);
        }
    }
    
    // Check if the context is valid
    bool is_valid() const {
        return window != nullptr && gl_context != nullptr;
    }

    std::pair<int, int> get_size() const {
        int width = 0, height = 0;
        if (window) {
            SDL_GetWindowSize(window, &width, &height);
        }
        return {width, height};
    }

    float get_aspect_ratio() const {
        auto [width, height] = get_size();
        return (height != 0) ? static_cast<float>(width) / height : 1.0f; // Avoid division by zero
    }
};

// Base interface for any system that participates in the event loop.
class IRenderableEntity {
public:
    virtual ~IRenderableEntity();
    virtual bool is_ready() = 0;
    virtual void render();
    virtual void present();

    virtual void activate_render_context();

    // Function to calculate FPS for rendering
    virtual float get_render_fps() const;

    // Function to calculate FPS for presentation
    virtual float get_present_fps() const;

    // SDL window/context initialization
    virtual bool initialize_sdl(
        unsigned int width, 
        unsigned int height, 
        const std::string& title = "OpenGL Window", 
        Uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN,
        bool visible = true
    );

    SDL_Window* get_window() const;
    SDL_GLContext get_context() const;
    unsigned int get_window_id() const;
    
    // Get the render context
    const RenderContext& get_render_context() const { return m_render_context; }
    
protected:
    void update_render_fps();
    void update_present_fps();
    void cleanup_sdl();

    // Render context
    RenderContext m_render_context;

    // Keeping these for backward compatibility during transition
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_context = nullptr;
    std::string m_title;

private:
    float m_render_fps{0.0f};
    float m_present_fps{0.0f};
    Uint32 m_last_render_time{0};
    Uint32 m_last_present_time{0};
    int m_render_frame_count{0};
    int m_present_frame_count{0};
    bool m_visible;
};

#endif // ENGINE_RENDERABLE_ITEM_H