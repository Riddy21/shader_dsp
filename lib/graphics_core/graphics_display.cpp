#include <SDL2/SDL.h>
#include <iostream>
#include <thread>
#include <string>

#include "graphics_core/graphics_display.h"
#include "graphics_core/graphics_view.h"
#include "engine/event_loop.h"
#include "graphics_core/graphics_component.h"

GLuint m_shaderProgram;

GraphicsDisplay::GraphicsDisplay(unsigned int width, unsigned int height, const std::string& title, unsigned int refresh_rate, EventHandler& event_handler)
    : m_width(width), m_height(height), m_title(title), m_refresh_rate(refresh_rate), m_event_handler(event_handler) {

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        // If already initialized, don't throw an error
        if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
            SDL_InitSubSystem(SDL_INIT_VIDEO);
        } else {
            std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
            throw std::runtime_error("SDL initialization failed");
        }
    }

    // Use the parent class method to initialize SDL window and context
    if (!initialize_sdl(width, height, title, SDL_WINDOW_SHOWN)) {
        throw std::runtime_error("SDL initialization failed");
    }

    if (SDL_GL_SetSwapInterval(0) < 0) { // Disable VSync
        std::cerr << "Warning: Unable to disable VSync: " << SDL_GetError() << std::endl;
    }

    set_vsync_enabled(false);

    // TODO: Consdier putting this in Constructor as a default
    auto& event_loop = EventLoop::get_instance();
    event_loop.add_loop_item(this); // Register this graphics display instance with the event loop
}

GraphicsDisplay::~GraphicsDisplay() {
    // Base class destructor will handle SDL cleanup
}

void GraphicsDisplay::add_view(const std::string& name, GraphicsView* view) {
    // TODO: Think of way to avoid this by making it RAII
    activate_render_context();
    m_views[name] = std::unique_ptr<GraphicsView>(view);
    m_views[name]->initialize(m_event_handler, this->get_render_context());
    unactivate_render_context();
}

void GraphicsDisplay::change_view(const std::string& name) {
    auto it = m_views.find(name);

    if (it != m_views.end()) {
        if (m_current_view) {
            m_current_view->on_exit();
        }
        m_current_view = it->second.get();
        m_current_view->on_enter();
    }
}

// Render graphics less often
bool GraphicsDisplay::is_ready() {
    Uint32 current_time = SDL_GetTicks();
    Uint32 frame_duration = 1000 / m_refresh_rate; // Calculate frame duration in milliseconds
    if (current_time - m_last_render_time >= frame_duration) {
        m_last_render_time = current_time;
        return get_window() != nullptr && get_context() != nullptr;
    }
    return false;
}

void GraphicsDisplay::render() {
    IRenderableEntity::render(); // Call the base class render to update FPS
    if (m_current_view) {
        m_current_view->render();
    }

    // Render additional components like the graph
    for (auto& component : m_components) {
        component->render();
    }
}