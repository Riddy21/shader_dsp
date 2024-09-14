#include <cstring>

#include "audio_int_parameter.h"

unsigned int AudioIntParameter::total_binding_points = 0;

bool AudioIntParameter::initialize_parameter() {

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
        glBufferData(GL_UNIFORM_BUFFER, sizeof(int), m_data->get_data(), GL_DYNAMIC_COPY);

    } else if (connection_type == ConnectionType::INITIALIZATION) {
        // Allocate memory for the buffer and data
        glBufferData(GL_UNIFORM_BUFFER, sizeof(int), m_data->get_data(), GL_STATIC_COPY);
    } else {
        // Output int parameters are not allowed
        printf("Error: output and passthrough int parameters are not allowed in parameter %s\n", name);
        return false;
    }

    GLenum status = glGetError();
    if (status != GL_NO_ERROR) {
        printf("Error: OpenGL error in parameter %s\n", name);
        return false;
    }
    
    // Unbind the buffer
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    return true;
}

bool AudioIntParameter::set_value(const void * value_ptr) {
    if (connection_type == ConnectionType::OUTPUT) {
        printf("Error: Cannot set value for output parameter %s\n", name);
        return false;
    }
    if (connection_type == ConnectionType::PASSTHROUGH) {
        printf("Error: Cannot set value for passthrough parameter %s\n", name);
        return false;
    }

    if (m_data == nullptr) {
        m_data = std::make_unique<ParamIntData>();
    }
    std::memcpy(m_data->get_data(), value_ptr, sizeof(int));
    return true;
}

void AudioIntParameter::render_parameter() {
    if (connection_type == ConnectionType::OUTPUT || connection_type == ConnectionType::PASSTHROUGH) {
        return; // Do not need to render if output
    }

    glUniform1i(glGetUniformLocation(m_render_stage_linked->get_shader_program(), name), m_render_stage_linked->get_texture_count());

    if (connection_type == ConnectionType::INPUT) {
        glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(int), m_data->get_data());
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

}

bool AudioIntParameter::bind_parameter() {
    glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);

    // Bind the buffer to the binding point
    glBindBufferBase(GL_UNIFORM_BUFFER, m_binding_point, m_ubo);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    GLenum status = glGetError();
    if (status != GL_NO_ERROR) {
        printf("Error: OpenGL error in parameter %s\n", name);
        return false;
    }
    return true;
}