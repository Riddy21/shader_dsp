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
    const std::string& label,
    ButtonCallback callback,
    IRenderableEntity* render_context
) : GraphicsComponent(x, y, width, height, render_context),
    m_label(label),
    m_callback(callback)
{
    initialize_graphics();
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
        
        uniform vec2 uPosition;
        uniform vec2 uDimensions;
        
        void main() {
            // Map from button local coordinates to screen coordinates
            vec2 pos = aPos * uDimensions + uPosition;
            gl_Position = vec4(pos, 0.0, 1.0);
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

void ButtonComponent::render() {
    glUseProgram(m_shader_program->get_program());
    
    // Set position and dimensions in normalized device coordinates
    glUniform2f(glGetUniformLocation(m_shader_program->get_program(), "uPosition"), m_x, m_y);
    glUniform2f(glGetUniformLocation(m_shader_program->get_program(), "uDimensions"), m_width, m_height);
    
    // Set color based on button state
    float* color = m_color;
    if (m_is_pressed) {
        color = m_active_color;
    } else if (m_is_hovered) {
        color = m_hover_color;
    }
    glUniform4f(glGetUniformLocation(m_shader_program->get_program(), "uColor"), 
                color[0], color[1], color[2], color[3]);
    
    // Draw the button
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    
    glUseProgram(0);
}

void ButtonComponent::set_label(const std::string& label) {
    m_label = label;
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

void ButtonComponent::register_event_handlers(EventHandler* event_handler) {
    // Store reference to the event handler
    m_event_handler = event_handler;

    if (!m_event_handler || !m_render_context) {
        printf("Error: Event handler or render context is not set for ButtonComponent\n");
        throw std::runtime_error("Event handler or render context is not set");
    }
    
    // Convert normalized device coordinates to window coordinates based on component size
    int screen_width = SDL_GetWindowSurface(SDL_GL_GetCurrentWindow())->w;
    int screen_height = SDL_GetWindowSurface(SDL_GL_GetCurrentWindow())->h;
    
    int window_x = (int)((m_x + 1.0f) * screen_width / 2);
    int window_y = (int)((1.0f - m_y) * screen_height / 2);
    int window_width = (int)(m_width * screen_width);
    int window_height = (int)(m_height * screen_height);
    
    // Calculate the button's rectangle in screen coordinates
    int rect_x = window_x - window_width / 2;
    int rect_y = window_y - window_height / 2;
    
    // Register mouse down handler for this button
    auto* mouse_down_handler = new MouseClickEventHandlerEntry(
        m_render_context,
        SDL_MOUSEBUTTONDOWN,
        rect_x, rect_y, window_width, window_height,
        [this](const SDL_Event& event) {
            m_is_pressed = true;
            return true;
        }
    );
    m_event_handler->register_entry(mouse_down_handler);
    m_event_handler_entries.push_back(mouse_down_handler);
    
    // Register mouse up handler for this button globally
    auto* mouse_up_handler = new MouseClickEventHandlerEntry(
        m_render_context,
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
        }
    );
    m_event_handler->register_entry(mouse_up_handler);
    m_event_handler_entries.push_back(mouse_up_handler);
    
    // Register mouse motion handler for hover effect
    auto* mouse_motion_handler = new MouseMotionEventHandlerEntry(
        m_render_context,
        rect_x, rect_y, window_width, window_height,
        [this](const SDL_Event& event) {
            m_is_hovered = true;
            return true;
        }
    );
    m_event_handler->register_entry(mouse_motion_handler);
    m_event_handler_entries.push_back(mouse_motion_handler);
    
    // Register mouse enter handler
    auto* mouse_enter_handler = new MouseEnterLeaveEventHandlerEntry(
        m_render_context,
        rect_x, rect_y, window_width, window_height,
        MouseEnterLeaveEventHandlerEntry::Mode::ENTER,
        [this](const SDL_Event& event) {
            m_is_hovered = true;
            return true;
        }
    );
    m_event_handler->register_entry(mouse_enter_handler);
    m_event_handler_entries.push_back(mouse_enter_handler);
    
    // Register mouse leave handler
    auto* mouse_leave_handler = new MouseEnterLeaveEventHandlerEntry(
        m_render_context,
        rect_x, rect_y, window_width, window_height,
        MouseEnterLeaveEventHandlerEntry::Mode::LEAVE,
        [this](const SDL_Event& event) {
            m_is_hovered = false;
            return true;
        }
    );
    m_event_handler->register_entry(mouse_leave_handler);
    m_event_handler_entries.push_back(mouse_leave_handler);
}

void ButtonComponent::unregister_event_handlers() {
    // If we have an event handler, unregister all our entries
    if (m_event_handler) {
        for (auto* entry : m_event_handler_entries) {
            m_event_handler->unregister_entry(entry);
        }
        m_event_handler_entries.clear();
    }
    
    m_event_handler = nullptr;
}