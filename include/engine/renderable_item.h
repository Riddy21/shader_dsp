#ifndef ENGINE_RENDERABLE_ITEM_H
#define ENGINE_RENDERABLE_ITEM_H

#include <SDL2/SDL.h> // For SDL_GetTicks
#include <cstdint>    // For standard integer types like Uint32
#include <string>

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
    
    
protected:
    void update_render_fps();
    void update_present_fps();
    void cleanup_sdl();

    // SDL window/context
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