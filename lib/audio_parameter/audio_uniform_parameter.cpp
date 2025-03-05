#include <stdexcept>
#include "audio_parameter/audio_uniform_parameter.h"

AudioUniformParameter::AudioUniformParameter(const std::string name,
                                             AudioParameter::ConnectionType connection_type)
    : AudioParameter(name, connection_type)
{
    if (connection_type == ConnectionType::OUTPUT && connection_type == ConnectionType::PASSTHROUGH) {
        printf("Error: Cannot set parameter %s as OUTPUT or PASSTHROUGH\n", name.c_str());
        std::string error_message = "Cannot set parameter " + name + " as OUTPUT or PASSTHROUGH";
        throw std::invalid_argument(error_message);
    }
}

void AudioUniformParameter::render() {
    // Check if the parameter is an input or initialization parameter
    if (connection_type == ConnectionType::INPUT || (connection_type == ConnectionType::INITIALIZATION && !m_initialized && m_update_param)) {
        // Get the location of the uniform
        GLuint location = glGetUniformLocation(m_shader_program_linked->get_program(), name.c_str());
        // Set the uniform
        set_uniform(location);
        // Set the initialized flag
        m_initialized = true;
        m_update_param = false;
    }
}   