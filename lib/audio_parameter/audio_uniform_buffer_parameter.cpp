#include <cstring>
#include <stdexcept>

#include "audio_parameter/audio_uniform_buffer_parameter.h"

unsigned int AudioUniformBufferParameter::total_binding_points = 0;

AudioUniformBufferParameter::AudioUniformBufferParameter(const std::string name,
                                                         AudioParameter::ConnectionType connection_type)
    : AudioParameter(name, connection_type),
      m_binding_point(total_binding_points++)
{
    // Cannot set value for output or passthrough parameters
    if (connection_type == ConnectionType::OUTPUT || connection_type == ConnectionType::PASSTHROUGH) {
        printf("Error: Cannot set parameter %s as OUTPUT or PASSTHROUGH\n", name.c_str());
        std::string error_message = "Cannot set parameter " + name + " as OUTPUT or PASSTHROUGH";
        throw std::invalid_argument(error_message);
    }
}

bool AudioUniformBufferParameter::initialize(GLuint frame_buffer, AudioShaderProgram * shader_program) {
    m_framebuffer_linked = frame_buffer;
    m_shader_program_linked = shader_program;

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
        printf("Error: OpenGL error in initializing parameter %s\n", name.c_str());
        return false;
    }

    // Look for the buffer in the shader program
    // NOTE: There is no way of checking if it's a global parameter, that should be ok
    if (m_shader_program_linked != nullptr) {
        auto location = glGetUniformBlockIndex(m_shader_program_linked->get_program(), name.c_str());
        if (location == GL_INVALID_INDEX) {
            printf("Error: Could not find buffer in shader program in parameter %s\n", name.c_str());
            return false;
        }
    }
    
    // Unbind the buffer
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    return true;
}

void AudioUniformBufferParameter::render() {
    if (connection_type == ConnectionType::INPUT) {
        glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);
        if (m_update_param) {
            glBufferSubData(GL_UNIFORM_BUFFER, 0, m_data->get_size(), m_data->get_data());
            m_update_param = false;
        }
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

}