
#include "audio_parameter/audio_uniform_parameter.h"

AudioUniformParameter::AudioUniformParameter(const std::string name,
                                             AudioParameter::ConnectionType connection_type)
    : AudioParameter(name, connection_type)
{
    if (connection_type == ConnectionType::OUTPUT && connection_type == ConnectionType::PASSTHROUGH) {
        char error_message[100];
        sprintf(error_message, "Error: Cannot set parameter %s as OUTPUT or PASSTHROUGH\n", name.c_str());
        throw std::invalid_argument(error_message);
    }
}

void AudioUniformParameter::render_parameter() {
    // Check if the parameter is an input or initialization parameter
    if (connection_type == ConnectionType::INPUT || (connection_type == ConnectionType::INITIALIZATION && !m_initialized)) {
        // Get the location of the uniform
        GLuint location = glGetUniformLocation(m_shader_program_linked->get_program(), name.c_str());
        // Set the uniform
        set_uniform(location);
        // Set the initialized flag
        m_initialized = true;
    }
}   