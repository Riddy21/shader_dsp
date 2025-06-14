#include <iostream>
#include <GL/glew.h>
#include "graphics_components/button_component.h"
#include "utilities/shader_program.h"
#include "audio_synthesizer/audio_synthesizer.h"
#include "engine/event_handler.h"
#include "engine/renderable_item.h"

ButtonComponent::ButtonComponent(
    float x, 
    float y, 
    float width, 
    float height, 
    ButtonCallback callback
) : GraphicsComponent(x, y, width, height),
    m_callback(callback)
{
    initialize_graphics();

    // Get window size at construction time
    int screen_width = 1, screen_height = 1;
    SDL_Window* win = SDL_GL_GetCurrentWindow();
    if (win) {
        SDL_Surface* surf = SDL_GetWindowSurface(win);
        if (surf) {
            screen_width = surf->w;
            screen_height = surf->h;
        }
    }
    
    // Convert from normalized coordinates (-1 to 1, with -1 at top and 1 at bottom)
    // to SDL event coordinates (0,0 top-left to width,height bottom-right)
    int window_x = (int)((m_x + 1.0f) * screen_width / 2);
    // For Y, -1 is at top and 1 is at bottom, so we need to invert
    int window_y = (int)((1.0f - m_y) * screen_height / 2);
    int window_width = (int)(m_width * screen_width / 2);
    int window_height = (int)(m_height * screen_height / 2);

    int rect_x = window_x;
    int rect_y = window_y;

    auto mouse_down_handler = std::make_shared<MouseClickEventHandlerEntry>(
        SDL_MOUSEBUTTONDOWN,
        rect_x, rect_y, window_width, window_height,
        [this](const SDL_Event& event) {
            m_is_pressed = true;
            return true;
        },
        m_render_context
    );

    auto mouse_up_handler = std::make_shared<MouseClickEventHandlerEntry>(
        SDL_MOUSEBUTTONUP,
        0, 0, screen_width, screen_height,
        [this](const SDL_Event& event) {
            if (m_is_pressed) {
                m_is_pressed = false;
                if (m_callback) {
                    m_callback();
                }
            }
            return true;
        },
        m_render_context
    );

    auto mouse_motion_handler = std::make_shared<MouseMotionEventHandlerEntry>(
        rect_x, rect_y, window_width, window_height,
        [this](const SDL_Event& event) {
            m_is_hovered = true;
            return true;
        },
        m_render_context
    );

    auto mouse_enter_handler = std::make_shared<MouseEnterLeaveEventHandlerEntry>(
        rect_x, rect_y, window_width, window_height,
        MouseEnterLeaveEventHandlerEntry::Mode::ENTER,
        [this](const SDL_Event& event) {
            m_is_hovered = true;
            return true;
        },
        m_render_context
    );

    auto mouse_leave_handler = std::make_shared<MouseEnterLeaveEventHandlerEntry>(
        rect_x, rect_y, window_width, window_height,
        MouseEnterLeaveEventHandlerEntry::Mode::LEAVE,
        [this](const SDL_Event& event) {
            m_is_hovered = false;
            return true;
        },
        m_render_context
    );

    m_event_handler_entries = {
        mouse_down_handler,
        mouse_up_handler,
        mouse_motion_handler,
        mouse_enter_handler,
        mouse_leave_handler
    };
}

ButtonComponent::~ButtonComponent() {
    // Clean up OpenGL resources
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
    }
    
    // Clean up event handler entries
    unregister_event_handlers();
}

void ButtonComponent::initialize_graphics() {
    // Simple vertex and fragment shaders for drawing a rectangle
    const std::string vertex_shader_src = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

    const std::string fragment_shader_src = R"(
        #version 330 core
        out vec4 FragColor;
        
        uniform vec4 uColor;
        
        void main() {
            FragColor = uColor;
        }
    )";

    m_shader_program = std::make_unique<AudioShaderProgram>(vertex_shader_src, fragment_shader_src);
    if (!m_shader_program->initialize()) {
        throw std::runtime_error("Failed to initialize shader program for ButtonComponent");
    }

    // Create a rectangle shape (two triangles)
    // Vertices in normalized device coordinates (from -1 to 1)
    float vertices[] = {
        -1.0f, -1.0f,  // bottom left
        -1.0f,  1.0f,  // top left
         1.0f,  1.0f,  // top right
        
        -1.0f, -1.0f,  // bottom left
         1.0f,  1.0f,  // top right
         1.0f, -1.0f   // bottom right
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void ButtonComponent::render_content() {
    glUseProgram(m_shader_program->get_program());
    
    // Set color based on button state
    float* color = m_color;
    if (m_is_pressed) {
        color = m_active_color;
    } else if (m_is_hovered) {
        color = m_hover_color;
    }
    glUniform4f(glGetUniformLocation(m_shader_program->get_program(), "uColor"), 
                color[0], color[1], color[2], color[3]);
    
    // Draw the button background
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    
    glUseProgram(0);
    
    // Update child components based on button state
    update_children();
}

void ButtonComponent::set_callback(ButtonCallback callback) {
    m_callback = callback;
}

void ButtonComponent::set_colors(float r, float g, float b, float a) {
    m_color[0] = r;
    m_color[1] = g;
    m_color[2] = b;
    m_color[3] = a;
}

void ButtonComponent::set_hover_colors(float r, float g, float b, float a) {
    m_hover_color[0] = r;
    m_hover_color[1] = g;
    m_hover_color[2] = b;
    m_hover_color[3] = a;
}

void ButtonComponent::set_active_colors(float r, float g, float b, float a) {
    m_active_color[0] = r;
    m_active_color[1] = g;
    m_active_color[2] = b;
    m_active_color[3] = a;
}

void ButtonComponent::update_children() {
    // Override this in derived classes to update child components based on button state
}