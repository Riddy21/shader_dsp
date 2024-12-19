#include <cstring>

#include "audio_parameter/audio_uniform_buffer_parameter.h"

unsigned int AudioUniformBufferParameter::total_binding_points = 0;

AudioUniformBufferParameter::AudioUniformBufferParameter(const std::string name,
                                                         AudioParameter::ConnectionType connection_type)
    : AudioParameter(name, connection_type),
      m_binding_point(total_binding_points++)
{
    // Cannot set value for output or passthrough parameters
    if (connection_type == ConnectionType::OUTPUT || connection_type == ConnectionType::PASSTHROUGH) {
        char error_message[256];
        sprintf(error_message, "Error: Cannot set parameter %s as OUTPUT or PASSTHROUGH\n", name.c_str());
        throw std::invalid_argument(error_message);
    }
}

bool AudioUniformBufferParameter::initialize_parameter() {

    // Generate a buffer
    glGenBuffers(1, &m_ubo);

    // Bind the buffer
    glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);

    // Allocate memory for the buffer
    if (connection_type == ConnectionType::INPUT) {
        // Allocate memory for the buffer and data
        if (m_data == nullptr) {
            printf("Warning: value is nullptr when declared as input or initialization in parameter %s\n", name.c_str());
        }
        glBufferData(GL_UNIFORM_BUFFER, m_data->get_size(), m_data->get_data(), GL_DYNAMIC_DRAW);

    } else if (connection_type == ConnectionType::INITIALIZATION) {
        // Allocate memory for the buffer and data
        glBufferData(GL_UNIFORM_BUFFER, m_data->get_size(), m_data->get_data(), GL_STATIC_DRAW);
    } else {
        // Output int parameters are not allowed
        printf("Error: output and passthrough int parameters are not allowed in parameter %s\n", name.c_str());
        return false;
    }

    // Bind the buffer to the binding point
    glBindBufferBase(GL_UNIFORM_BUFFER, m_binding_point, m_ubo);

    GLenum status = glGetError();
    if (status != GL_NO_ERROR) {
        printf("Error: OpenGL error in initializing parameter %s\n", name);
        return false;
    }

    // Look for the buffer in the shader program
    if (m_render_stage_linked != nullptr) {
        auto location = glGetUniformBlockIndex(m_render_stage_linked->get_shader_program(), name.c_str());
        if (location == GL_INVALID_INDEX) {
            printf("Error: Could not find buffer in shader program in parameter %s\n", name.c_str());
            return false;
        }
    }
    
    // Unbind the buffer
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    return true;
}

void AudioUniformBufferParameter::render_parameter() {
    if (connection_type == ConnectionType::INPUT) {
        glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(int), m_data->get_data());
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

}

bool AudioUniformBufferParameter::bind_parameter() {
    glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);

    GLuint block_index = glGetUniformBlockIndex(m_render_stage_linked->get_shader_program(), name.c_str());

    if (block_index == GL_INVALID_INDEX) {
        printf("Error: Uniform block index not found in parameter %s\n", name.c_str());
        return false;
    }

    glUniformBlockBinding(m_render_stage_linked->get_shader_program(), block_index, m_binding_point);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    GLenum status = glGetError();
    if (status != GL_NO_ERROR) {
        printf("Error: OpenGL error %d in binding parameter %s\n", status, name.c_str());
        return false;
    }
    return true;
}
