
#include "audio_uniform_parameters.h"

AudioUniformParameter::AudioUniformParameter(const char * name,
                                             AudioParameter::ConnectionType connection_type)
    : AudioParameter(name, connection_type)
{
    if (connection_type == ConnectionType::OUTPUT && connection_type == ConnectionType::PASSTHROUGH) {
        char error_message[100];
        sprintf(error_message, "Error: Cannot set parameter %s as OUTPUT or PASSTHROUGH\n", name);
        throw std::invalid_argument(error_message);
    }
}

void AudioUniformParameter::render_parameter() {
    // Check if the parameter is an input or initialization parameter
    if (connection_type == ConnectionType::INPUT || (connection_type == ConnectionType::INITIALIZATION && !m_initialized)) {
        // Get the location of the uniform
        GLuint location = glGetUniformLocation(m_render_stage_linked->get_shader_program(), name);
        // Set the uniform
        set_uniform(location);
        // Set the initialized flag
        m_initialized = true;
    }
}   