#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <iostream>
#include <thread>
#include <string>

#include "graphics/graphics_display.h"
#include "graphics_core/graphics_view.h"
#include "engine/event_loop.h"

GLuint m_shaderProgram;

GraphicsDisplay::GraphicsDisplay(unsigned int width, unsigned int height, const std::string& title, unsigned int refresh_rate)
    : m_width(width), m_height(height), m_title(title), m_refresh_rate(refresh_rate) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        throw std::runtime_error("SDL initialization failed");
    }

    m_window = SDL_CreateWindow(
        m_title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        m_width,
        m_height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    if (!m_window) {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        throw std::runtime_error("SDL window creation failed");
    }

    m_context = SDL_GL_CreateContext(m_window);
    if (!m_context) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(m_window);
        SDL_Quit();
        throw std::runtime_error("OpenGL context creation failed");
    }

    SDL_GL_MakeCurrent(m_window, m_context); // Bind context to the main thread initially

    if (SDL_GL_SetSwapInterval(0) < 0) { // Disable VSync
        std::cerr << "Warning: Unable to disable VSync: " << SDL_GetError() << std::endl;
    }

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        SDL_GL_DeleteContext(m_context);
        SDL_DestroyWindow(m_window);
        SDL_Quit();
        throw std::runtime_error("GLEW initialization failed");
    }

    auto& event_loop = EventLoop::get_instance();
    event_loop.add_loop_item(this); // Register this graphics display instance with the event loop
}

GraphicsDisplay::~GraphicsDisplay() {
    if (m_context) {
        SDL_GL_DeleteContext(m_context);
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
    }
    SDL_Quit();
}

void GraphicsDisplay::register_view(const std::string& name, GraphicsView* view) {
    m_views[name] = std::unique_ptr<GraphicsView>(view);
}

void GraphicsDisplay::change_view(const std::string& name) {
    if (m_current_view) {
        m_current_view->on_exit();
    }
    auto it = m_views.find(name);
    if (it != m_views.end()) {
        m_current_view = it->second.get();
        m_current_view->on_enter();
    }
}

// Render graphics less often
bool GraphicsDisplay::is_ready() {
    static Uint32 last_time = 0;
    Uint32 current_time = SDL_GetTicks();
    Uint32 frame_duration = 1000 / m_refresh_rate; // Calculate frame duration in milliseconds
    if (current_time - last_time >= frame_duration) {
        last_time = current_time;
        return m_window != nullptr && m_context != nullptr;
    }
    return false;
}

bool GraphicsDisplay::handle_event(const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        EventLoop::get_instance().terminate();
    }
    if (m_current_view) {
        return m_current_view->handle_event(event);
    }
    return false;
}

void GraphicsDisplay::render() {
    SDL_GL_MakeCurrent(m_window, m_context); // Ensure this context is active

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_current_view) {
        m_current_view->render();
    }

    // Render additional components like the graph
    for (auto& component : m_components) {
        component->render();
    }

    IEventLoopItem::update_render_fps(); // Call the base class render to update FPS
}

void GraphicsDisplay::present() {
    SDL_GL_MakeCurrent(m_window, m_context); // Ensure this context is active

    SDL_GL_SwapWindow(m_window); // Swap buffers for this context

    IEventLoopItem::update_present_fps(); // Call the base class present to update FPS
}
