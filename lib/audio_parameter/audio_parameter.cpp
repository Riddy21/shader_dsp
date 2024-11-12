#include <cstring>

#include "audio_parameter/audio_parameter.h"

bool AudioParameter::set_value(const void * value_ptr) {
    // Only allow setting value if the parameter is an input
    if (connection_type == ConnectionType::PASSTHROUGH || connection_type == ConnectionType::OUTPUT) {
        printf("Error: Cannot set value for parameter %s\n", name);
        return false;
    }

    // Initialize m_value with enough memory
    if (m_data == nullptr) {
        m_data = create_param_data();
    }

    // Copy the value to m_value
    std::memcpy(m_data->get_data(), value_ptr, m_data->get_size());

    return true;
}