#include <iostream>
#include "graphics_components/button_component.h"
#include "utilities/shader_program.h"
#include "audio_synthesizer/audio_synthesizer.h"
#include "engine/event_handler.h"
#include "engine/renderable_entity.h"

ButtonComponent::ButtonComponent(
    float x, 
    float y, 
    float width, 
    float height, 
    ButtonCallback callback
) : GraphicsComponent(x, y, width, height),
    m_callback(callback)
{
    // OpenGL resource initialization moved to initialize() method

    // Mouse event handlers now accept normalized coordinates directly
    auto mouse_down_handler = std::make_shared<MouseClickEventHandlerEntry>(
        SDL_MOUSEBUTTONDOWN,
        m_x, m_y, m_width, m_height,
        [this](const SDL_Event& event) {
            m_is_pressed = true;
            return true;
        },
        m_render_context
    );

    auto mouse_up_handler = std::make_shared<MouseClickEventHandlerEntry>(
        SDL_MOUSEBUTTONUP,
        -1.0f, 1.0f, 2.0f, 2.0f,
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
        m_x, m_y, m_width, m_height,
        [this](const SDL_Event& event) {
            m_is_hovered = true;
            return true;
        },
        m_render_context
    );

    auto mouse_enter_handler = std::make_shared<MouseEnterLeaveEventHandlerEntry>(
        m_x, m_y, m_width, m_height,
        MouseEnterLeaveEventHandlerEntry::Mode::ENTER,
        [this](const SDL_Event& event) {
            m_is_hovered = true;
            return true;
        },
        m_render_context
    );

    auto mouse_leave_handler = std::make_shared<MouseEnterLeaveEventHandlerEntry>(
        m_x, m_y, m_width, m_height,
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

bool ButtonComponent::initialize() {
    // Simple vertex and fragment shaders for drawing a rectangle
    const std::string vertex_shader_src = R"(
        #version 300 es
        layout (location = 0) in vec2 aPos;
        
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

    const std::string fragment_shader_src = R"(
        #version 300 es
        precision mediump float;
        out vec4 FragColor;
        
        uniform vec4 uColor;
        
        void main() {
            FragColor = uColor;
        }
    )";

    m_shader_program = std::make_unique<AudioShaderProgram>(vertex_shader_src, fragment_shader_src);
    if (!m_shader_program->initialize()) {
        std::cerr << "Failed to initialize shader program for ButtonComponent" << std::endl;
        return false;
    }

    // Create a rectangle shape (two triangles for fill, four points for outline)
    // Vertices in normalized device coordinates (from -1 to 1)
    float vertices[] = {
        // Fill (Triangles)
        -1.0f, -1.0f,  // bottom left
        -1.0f,  1.0f,  // top left
         1.0f,  1.0f,  // top right
        
        -1.0f, -1.0f,  // bottom left
         1.0f,  1.0f,  // top right
         1.0f, -1.0f,  // bottom right
         
        // Outline (Line Loop)
        -1.0f, -1.0f,  // bottom left
        -1.0f,  1.0f,  // top left
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

    GraphicsComponent::initialize();
    
    return true;
}

void ButtonComponent::render_content() {
    if (!m_shader_program || m_vao == 0) return;
    
    glUseProgram(m_shader_program->get_program());
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Set color based on button state
    float* color = m_color;
    if (m_is_pressed) {
        color = m_active_color;
    } else if (m_is_hovered) {
        color = m_hover_color;
    }
    glUniform4f(glGetUniformLocation(m_shader_program->get_program(), "uColor"), 
                color[0], color[1], color[2], color[3]);
    
    glBindVertexArray(m_vao);
    
    // Draw the button background
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    // Draw border if enabled
    if (m_show_border) {
        glLineWidth(m_border_width);
        glUniform4f(glGetUniformLocation(m_shader_program->get_program(), "uColor"), 
                    m_border_color[0], m_border_color[1], m_border_color[2], m_border_color[3]);
        glDrawArrays(GL_LINE_LOOP, 6, 4);
    }
    
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    
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

void ButtonComponent::set_border_color(float r, float g, float b, float a) {
    m_border_color[0] = r;
    m_border_color[1] = g;
    m_border_color[2] = b;
    m_border_color[3] = a;
}

void ButtonComponent::set_border_visible(bool visible) {
    m_show_border = visible;
}

void ButtonComponent::set_border_width(float width) {
    m_border_width = width;
}

void ButtonComponent::update_children() {
    // Override this in derived classes to update child components based on button state
}