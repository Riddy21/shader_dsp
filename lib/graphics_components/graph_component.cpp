#include <iostream>
#include <GL/glew.h>
#include "graphics_components/graph_component.h"
#include "utilities/shader_program.h"

GraphComponent::GraphComponent(
    const float x, 
    const float y, 
    const float width, 
    const float height, 
    const std::vector<float>& data,
    const bool is_dynamic,
    EventHandler* event_handler,
    const RenderContext& render_context
) : GraphicsComponent(x, y, width, height, event_handler, render_context),
    m_data(&data),
    m_shader_program(nullptr),
    m_vao(0),
    m_vbo(0),
    m_is_dynamic(is_dynamic)
{
    // Initialize shader program using AudioShaderProgram
    const std::string vertex_shader_src = R"(
        #version 330 core
        layout(location = 0) in float value;
        uniform float data_size;
        void main() {
            float x = gl_VertexID / (data_size - 1) * 2.0 - 1.0;
            float y = value; // Already in [-1, 1] range
            gl_Position = vec4(x, y, 0.0, 1.0);
        }
    )";

    const std::string fragment_shader_src = R"(
        #version 330 core
        out vec4 frag_color;
        void main() {
            frag_color = vec4(0.0, 1.0, 0.0, 1.0); // Green color
        }
    )";

    m_shader_program = std::make_unique<AudioShaderProgram>(vertex_shader_src, fragment_shader_src);
    if (!m_shader_program->initialize()) {
        throw std::runtime_error("Failed to initialize shader program for GraphComponent");
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    set_data(data);
}

GraphComponent::~GraphComponent() {
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
}

void GraphComponent::set_data(const std::vector<float>& data) {
    m_data = &data;

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    if (m_is_dynamic) {
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), m_data, GL_DYNAMIC_DRAW);
    } else {
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), m_data, GL_STATIC_DRAW);
    }   
}

void GraphComponent::render_content() {
    if (!m_data->size()) return;

    // Use the shader program with simplified coordinate system
    glUseProgram(m_shader_program->get_program());
    glUniform1f(glGetUniformLocation(m_shader_program->get_program(), "data_size"), 
                static_cast<float>(m_data->size()));

    // Bind and update vertex data if needed
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    if (m_is_dynamic) {
        glBufferData(GL_ARRAY_BUFFER, m_data->size() * sizeof(float), m_data->data(), GL_DYNAMIC_DRAW);
    }

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, nullptr);

    glLineWidth(1.0f);
    glDrawArrays(GL_LINE_STRIP, 0, m_data->size());

    // Clean up
    glDisableVertexAttribArray(0);
    glBindVertexArray(0);
    glUseProgram(0);
}