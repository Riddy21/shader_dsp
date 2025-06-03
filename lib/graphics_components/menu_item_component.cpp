#include <iostream>
#include <GL/glew.h>
#include "graphics_components/menu_item_component.h"
#include "utilities/shader_program.h"
#include "engine/event_handler.h"

MenuItemComponent::MenuItemComponent(
    float x, float y, float width, float height,
    const std::string& text, int item_index
) : GraphicsComponent(x, y, width, height),
    m_index(item_index)
{
    initialize_graphics();
    
    // Create the text component as a child
    m_text_component = new TextComponent(x, y, width, height, text);
    m_text_component->set_horizontal_alignment(0.5f); // Center text
    m_text_component->set_vertical_alignment(0.5f);
    m_text_component->set_text_color(
        m_normal_text_color[0], 
        m_normal_text_color[1], 
        m_normal_text_color[2], 
        m_normal_text_color[3]
    );
    
    add_child(m_text_component);
}

MenuItemComponent::~MenuItemComponent() {
    // Clean up OpenGL resources
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
    }
    
    // Child components are cleaned up by the base class
}

void MenuItemComponent::initialize_graphics() {
    // Simple vertex and fragment shaders for drawing a rectangle
    const std::string vertex_shader_src = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        
        void main() {
            // aPos is already in [-1, 1] range
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
        throw std::runtime_error("Failed to initialize shader program for MenuItemComponent");
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

void MenuItemComponent::render_content() {
    glUseProgram(m_shader_program->get_program());

    if (m_colors_dirty) {
        update_colors();
        m_colors_dirty = false;
    }
    
    // Set color based on selected state
    float* color = m_is_selected ? m_selected_color : m_normal_color;

    glUniform4f(glGetUniformLocation(m_shader_program->get_program(), "uColor"), 
                color[0], color[1], color[2], color[3]);
    
    // Draw the menu item background
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    
    glUseProgram(0);
}

void MenuItemComponent::set_selected(bool selected) {
    if (m_is_selected == selected) return;
    m_is_selected = selected;
    m_colors_dirty = true;
}

void MenuItemComponent::update_colors() {
    if (m_text_component) {
        if (m_is_selected) {
            m_text_component->set_text_color(
                m_selected_text_color[0],
                m_selected_text_color[1],
                m_selected_text_color[2],
                m_selected_text_color[3]
            );
        } else {
            m_text_component->set_text_color(
                m_normal_text_color[0],
                m_normal_text_color[1],
                m_normal_text_color[2],
                m_normal_text_color[3]
            );
        }
    }
}

void MenuItemComponent::set_text(const std::string& text) {
    if (m_text_component) {
        m_text_component->set_text(text);
    }
}

const std::string& MenuItemComponent::get_text() const {
    static std::string empty_string;
    return m_text_component ? m_text_component->get_text() : empty_string;
}

void MenuItemComponent::set_normal_color(float r, float g, float b, float a) {
    m_normal_color[0] = r;
    m_normal_color[1] = g;
    m_normal_color[2] = b;
    m_normal_color[3] = a;
    m_colors_dirty = true;
}

void MenuItemComponent::set_selected_color(float r, float g, float b, float a) {
    m_selected_color[0] = r;
    m_selected_color[1] = g;
    m_selected_color[2] = b;
    m_selected_color[3] = a;
    m_colors_dirty = true;
}

void MenuItemComponent::set_normal_text_color(float r, float g, float b, float a) {
    m_normal_text_color[0] = r;
    m_normal_text_color[1] = g;
    m_normal_text_color[2] = b;
    m_normal_text_color[3] = a;
    m_colors_dirty = true;
}

void MenuItemComponent::set_selected_text_color(float r, float g, float b, float a) {
    m_selected_text_color[0] = r;
    m_selected_text_color[1] = g;
    m_selected_text_color[2] = b;
    m_selected_text_color[3] = a;
    m_colors_dirty = true;
}

void MenuItemComponent::set_font_size(int size) {
    if (m_text_component) {
        m_text_component->set_font_size(size);
    }
}

bool MenuItemComponent::set_font(const std::string& font_name) {
    if (m_text_component) {
        return m_text_component->set_font(font_name);
    }
    return false;
}