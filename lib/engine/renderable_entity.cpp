#include <SDL2/SDL.h>
#include <iostream>
#include <stdexcept>

#include "engine/renderable_entity.h"
#include "utilities/egl_compatibility.h"

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
    bool visible,
    bool vsync_enabled
) {
    m_visible = visible; // Store visibility state
    m_vsync_enabled = vsync_enabled;
    m_title = title;
    std::cout << "Initializing SDL window: " << title << std::endl;
    
    // Create flags based on visibility
    if (!visible) {
        window_flags = (window_flags & ~SDL_WINDOW_SHOWN) | SDL_WINDOW_HIDDEN;
    }
    
    // Remove SDL_WINDOW_OPENGL flag since we'll use EGL
    window_flags = window_flags & ~SDL_WINDOW_OPENGL;
    
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

    // Use EGL to create OpenGL ES context instead of SDL's OpenGL context
    if (!EGLCompatibility::initialize_egl_context(m_window, m_context)) {
        std::cerr << "Failed to create OpenGL ES context with EGL" << std::endl;
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        return false;
    }

    // Initialize the render context
    m_render_context = RenderContext(m_window, m_context, title, visible);

    // Apply the desired VSync setting
    set_vsync_enabled(m_vsync_enabled);

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
    
    if (m_render_context.window) {
        EGLCompatibility::cleanup_egl_context(m_render_context.window);
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
        // Use EGL swap buffers instead of SDL
        EGLCompatibility::swap_buffers(m_render_context.window);
    }

    // Update the present FPS
    update_present_fps();
}

void IRenderableEntity::set_vsync_enabled(bool enabled) {
    m_vsync_enabled = enabled;
    activate_render_context();

    // Try SDL path first (may fail when using pure EGL)
    if (SDL_GL_SetSwapInterval(enabled ? 1 : 0) < 0) {
        std::cerr << "Warning: Unable to set VSync via SDL: " << SDL_GetError() << std::endl;
    }

    // Fallback / ensure using EGL layer (also stores interval for later)
    EGLCompatibility::set_swap_interval(m_render_context.window, enabled ? 1 : 0);
}