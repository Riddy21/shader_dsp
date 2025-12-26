#include <iostream>
#include "graphics_components/graph_component.h"
#include "utilities/shader_program.h"

GraphComponent::GraphComponent(
    PositionMode position_mode,
    const float x, 
    const float y, 
    const float width, 
    const float height, 
    const std::vector<float>& data,
    const bool is_dynamic
) : GraphicsComponent(position_mode, x, y, width, height),
    m_data(&data),
    m_shader_program(nullptr),
    m_vao(0),
    m_vbo(0),
    m_is_dynamic(is_dynamic)
{
    // OpenGL resource initialization moved to initialize() method
}

GraphComponent::GraphComponent(
    const float x, 
    const float y, 
    const float width, 
    const float height, 
    const std::vector<float>& data,
    const bool is_dynamic
) : GraphComponent(PositionMode::CORNER, x, y, width, height, data, is_dynamic)
{
}

GraphComponent::~GraphComponent() {
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
    }
}

bool GraphComponent::initialize() {
    // Initialize shader program using AudioShaderProgram
    const std::string vertex_shader_src = R"(
        #version 300 es
        layout(location = 0) in float value;
        uniform float data_size;
        void main() {
            float x = float(gl_VertexID) / (data_size - 1.0) * 2.0 - 1.0;
            float y = value; // Already in [-1, 1] range
            gl_Position = vec4(x, y, 0.0, 1.0);
        }
    )";

    const std::string fragment_shader_src = R"(
        #version 300 es
        precision mediump float;
        out vec4 frag_color;
        void main() {
            frag_color = vec4(0.0, 1.0, 0.0, 1.0); // Green color
        }
    )";

    m_shader_program = std::make_unique<AudioShaderProgram>(vertex_shader_src, fragment_shader_src);
    if (!m_shader_program->initialize()) {
        std::cerr << "Failed to initialize shader program for GraphComponent" << std::endl;
        return false;
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    // Initialize with current data
    set_data(*m_data);

    GraphicsComponent::initialize();
    
    return true;
}

void GraphComponent::set_data(const std::vector<float>& data) {
    m_data = &data;

    // Only update buffer if OpenGL resources are initialized
    if (m_vbo != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        if (m_is_dynamic) {
            glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), m_data->data(), GL_DYNAMIC_DRAW);
        } else {
            glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), m_data->data(), GL_STATIC_DRAW);
        }   
    }
}

void GraphComponent::render_content() {
    if (!m_data->size() || !m_shader_program || m_vao == 0) return;

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

    // Note: glLineWidth is not supported in OpenGL ES 3.0, line width is always 1.0
    glDrawArrays(GL_LINE_STRIP, 0, m_data->size());

    // Clean up
    glDisableVertexAttribArray(0);
    glBindVertexArray(0);
    glUseProgram(0);
}