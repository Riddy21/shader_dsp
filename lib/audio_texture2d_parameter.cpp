#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut.h>

#include "audio_texture2d_parameter.h"

const float AudioTexture2DParameter::FLAT_COLOR[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

void AudioTexture2DParameter::init() {
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, FLAT_COLOR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    if (connection_type == ConnectionType::INPUT || connection_type == ConnectionType::INITIALIZATION) {
        if (m_value == nullptr) {
            printf("Warning: value is nullptr when declared as input or initialization in parameter %s\n", name);
        }
        glTexImage2D(GL_TEXTURE_2D, 0, m_internal_format, m_parameter_width, m_parameter_height, 0, m_format, m_datatype, m_value);
    } else if (connection_type == ConnectionType::PASSTHROUGH) {
        glTexImage2D(GL_TEXTURE_2D, 0, m_internal_format, m_parameter_width, m_parameter_height, 0, m_format, m_datatype, nullptr);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

}

void AudioTexture2DParameter::update_shader() {
    glActiveTexture(GL_TEXTURE0 + m_render_stage_linked->get_texture_count());
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glUniform1i(glGetUniformLocation(m_render_stage_linked->get_shader_program(), name), m_render_stage_linked->get_texture_count());

    if (connection_type == ConnectionType::INPUT) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_parameter_width, m_parameter_height, m_format, m_datatype, m_value);
    } else if (connection_type == ConnectionType::PASSTHROUGH) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_parameter_width, m_parameter_height, m_format, m_datatype, nullptr);
    }

}

void AudioTexture2DParameter::set_value(const void * value_ptr) {
    const float * value = static_cast<const float *>(value_ptr);
    std::copy(value, value + m_parameter_width * m_parameter_height,
              static_cast<float *>(m_value));
}



