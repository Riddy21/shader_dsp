#include <cstring>

#include "audio_core/audio_parameter.h"

bool AudioParameter::set_value(const void * value_ptr) {
    // Only allow setting value if the parameter is an input
    if (connection_type == ConnectionType::PASSTHROUGH || connection_type == ConnectionType::OUTPUT) {
        printf("Error: Cannot set value for parameter %s\n", name.c_str());
        return false;
    }

    // Initialize m_value with enough memory
    if (m_data == nullptr) {
        m_data = create_param_data();
    }

    // Copy the value to m_value
    std::memcpy(m_data->get_data(), value_ptr, m_data->get_size());

    m_update_param = true;

    return true;
}

bool AudioParameter::link(AudioParameter * parameter) {
    if (parameter == nullptr) {
        return false;
    }

    m_linked_parameter = parameter;
    m_linked_parameter->m_previous_parameter = this;
    return true;
}

bool AudioParameter::unlink() {
    if (m_linked_parameter == nullptr) {
        return false;
    }

    m_linked_parameter->m_previous_parameter = nullptr;
    m_linked_parameter = nullptr;
    return true;
}