#include <cstring>

#include "audio_uniform_parameters.h"

unsigned int AudioUniformParameter::total_binding_points = 0;

bool AudioUniformParameter::initialize_parameter() {

    // Generate a buffer
    glGenBuffers(1, &m_ubo);

    // Bind the buffer
    glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);

    // Allocate memory for the buffer
    if (connection_type == ConnectionType::INPUT) {
        // Allocate memory for the buffer and data
        if (m_data == nullptr) {
            printf("Warning: value is nullptr when declared as input or initialization in parameter %s\n", name);
        }
        glBufferData(GL_UNIFORM_BUFFER, m_data->get_size(), m_data->get_data(), GL_DYNAMIC_DRAW);

    } else if (connection_type == ConnectionType::INITIALIZATION) {
        // Allocate memory for the buffer and data
        glBufferData(GL_UNIFORM_BUFFER, m_data->get_size(), m_data->get_data(), GL_STATIC_DRAW);
    } else {
        // Output int parameters are not allowed
        printf("Error: output and passthrough int parameters are not allowed in parameter %s\n", name);
        return false;
    }

    // Bind the buffer to the binding point
    glBindBufferBase(GL_UNIFORM_BUFFER, m_binding_point, m_ubo);

    GLenum status = glGetError();
    if (status != GL_NO_ERROR) {
        printf("Error: OpenGL error in parameter %s\n", name);
        return false;
    }
    
    // Unbind the buffer
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    return true;
}

void AudioUniformParameter::render_parameter() {
    if (connection_type == ConnectionType::OUTPUT || connection_type == ConnectionType::PASSTHROUGH) {
        return; // Do not need to render if output
    }

    if (connection_type == ConnectionType::INPUT) {
        glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(int), m_data->get_data());
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

}

bool AudioUniformParameter::bind_parameter() {
    glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);

    GLuint block_index = glGetUniformBlockIndex(m_render_stage_linked->get_shader_program(), name);

    if (block_index == GL_INVALID_INDEX) {
        printf("Error: Uniform block index not found in parameter %s\n", name);
        return false;
    }

    glUniformBlockBinding(m_render_stage_linked->get_shader_program(), block_index, m_binding_point);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    GLenum status = glGetError();
    if (status != GL_NO_ERROR) {
        printf("Error: OpenGL error in parameter %s\n", name);
        return false;
    }
    return true;
}
