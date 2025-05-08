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
    const bool is_dynamic
) : GraphicsComponent(x, y, width, height),
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
        uniform vec2 position;
        uniform vec2 dimensions;
        void main() {
            float x = gl_VertexID / (data_size - 1) * dimensions.x + position.x - 1.0;
            float y = (value * 0.5 + 0.5) * dimensions.y + position.y - 1.0; // Map value from [-1, 1] to [0, 1]
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

bool GraphComponent::handle_event(const SDL_Event& event) {
    // Handle knob-specific events
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        std::cout << "Knob clicked!" << std::endl;
        return true;
    }

    return false;
}

void GraphComponent::render() {
    if (!m_data->size()) return;

    glUseProgram(m_shader_program->get_program());
    glUniform1f(glGetUniformLocation(m_shader_program->get_program(), "data_size"), static_cast<float>(m_data->size()));
    glUniform2f(glGetUniformLocation(m_shader_program->get_program(), "position"), m_x, m_y);
    glUniform2f(glGetUniformLocation(m_shader_program->get_program(), "dimensions"), m_width, m_height);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    if (m_is_dynamic) {
        glBufferData(GL_ARRAY_BUFFER, m_data->size() * sizeof(float), m_data->data(), GL_DYNAMIC_DRAW);
    }

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, nullptr);

    glLineWidth(1.0f); // Set the line width to 2.0 (in pixels)
    glDrawArrays(GL_LINE_STRIP, 0, m_data->size());

    glDisableVertexAttribArray(0);
    glBindVertexArray(0);
    glUseProgram(0);
}
