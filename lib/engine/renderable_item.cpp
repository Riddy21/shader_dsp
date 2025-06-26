#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <iostream>
#include <stdexcept>

#include "engine/renderable_item.h"

IRenderableEntity::~IRenderableEntity() {
    cleanup_sdl();
}

void IRenderableEntity::activate_render_context() {
    m_render_context.activate();
}

void IRenderableEntity::unactivate_render_context() {
    m_render_context.unactivate();
}

float IRenderableEntity::get_render_fps() const { 
    return m_render_fps; 
}

float IRenderableEntity::get_present_fps() const { 
    return m_present_fps; 
}

bool IRenderableEntity::initialize_sdl(
    unsigned int width, 
    unsigned int height, 
    const std::string& title, 
    Uint32 window_flags,
    bool visible
) {
    m_visible = visible; // Store visibility state
    m_title = title;
    std::cout << "Initializing SDL window: " << title << std::endl;
    
    // Set OpenGL ES attributes before creating the window
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    
    // Create flags based on visibility
    if (!visible) {
        window_flags = (window_flags & ~SDL_WINDOW_SHOWN) | SDL_WINDOW_HIDDEN;
    }
    
    m_window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        window_flags
    );
    
    if (!m_window) {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << std::endl;
        return false;
    }

    m_context = SDL_GL_CreateContext(m_window);
    if (!m_context) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        return false;
    }

    SDL_GL_MakeCurrent(m_window, m_context); // Bind context to the current thread

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        cleanup_sdl();
        return false;
    }

    // Initialize the render context
    m_render_context = RenderContext(m_window, m_context, title, visible);

    return true;
}

SDL_Window* IRenderableEntity::get_window() const { 
    return m_render_context.window; 
}

SDL_GLContext IRenderableEntity::get_context() const { 
    return m_render_context.gl_context; 
}

unsigned int IRenderableEntity::get_window_id() const {
    return m_render_context.window_id;
}

void IRenderableEntity::update_render_fps() {
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

void IRenderableEntity::update_present_fps() {
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

void IRenderableEntity::cleanup_sdl() {
    activate_render_context();
    
    if (m_render_context.gl_context) {
        SDL_GL_MakeCurrent(m_render_context.window, m_render_context.gl_context);
        SDL_GL_DeleteContext(m_render_context.gl_context);
        m_context = nullptr;
        m_render_context.gl_context = nullptr;
    }
    
    if (m_render_context.window) {
        SDL_DestroyWindow(m_render_context.window);
        m_window = nullptr;
        m_render_context.window = nullptr;
    }
}

void IRenderableEntity::render() {
    activate_render_context();

    // No need to call activate_render_context() here, event loop will do it
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    // Update the render FPS
    update_render_fps();
}

void IRenderableEntity::present() {
    activate_render_context();

    if (m_render_context.visible) {
        // No need to call activate_render_context() here, event loop will do it
        SDL_GL_SwapWindow(m_render_context.window);
    }

    // Update the present FPS
    update_present_fps();
}