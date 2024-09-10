#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <cstring>

#include "audio_texture2d_parameter.h"

const float AudioTexture2DParameter::FLAT_COLOR[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

bool AudioTexture2DParameter::init() {
    if (m_render_stage_linked == nullptr) {
        printf("Error: render stage linked is nullptr in parameter %s\n", name);
        return false;
    }

    // Generate the texture
    glGenTextures(1, &m_texture);

    // Bind the texture
    glBindTexture(GL_TEXTURE_2D, m_texture);

    // Configure the texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, FLAT_COLOR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    // allocate memory for the texture
    if (connection_type == ConnectionType::INPUT || connection_type == ConnectionType::INITIALIZATION) {
        // Allocate memory for the texture and data 
        if (m_value == nullptr) {
            printf("Warning: value is nullptr when declared as input or initialization in parameter %s\n", name);
        }
        glTexImage2D(GL_TEXTURE_2D, 0, m_internal_format, m_parameter_width, m_parameter_height, 0, m_format, m_datatype, m_value);

    } else if (connection_type == ConnectionType::PASSTHROUGH || connection_type == ConnectionType::OUTPUT) {
        // Allocate memory for the texture and data but don't update the data
        glTexImage2D(GL_TEXTURE_2D, 0, m_internal_format, m_parameter_width, m_parameter_height, 0, m_format, m_datatype, nullptr);

    }
    
    // Link frame buffer to texture if output
    if (connection_type == ConnectionType::OUTPUT) {
        // bind the frame buffer
        glBindFramebuffer(GL_FRAMEBUFFER, m_render_stage_linked->get_framebuffer());
        // Link the texture to the framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + m_render_stage_linked->get_color_attachment_count(),
                               GL_TEXTURE_2D, m_texture, 0);
        // Draw the buffer
        GLenum draw_buffers[1] = { m_render_stage_linked->get_color_attachment_count() };
        glDrawBuffers(1, draw_buffers);
        // Unbind the framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Check for errors 
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        printf("Error: Framebuffer is not complete in parameter %s\n", name);
        return false;
    }
    status = glGetError();
    if (status != GL_NO_ERROR) {
        printf("Error: OpenGL error in parameter %s\n", name);
        return false;
    }

    // Unbind the texture
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void AudioTexture2DParameter::update_shader() {
    glActiveTexture(m_render_stage_linked->get_texture_count());
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glUniform1i(glGetUniformLocation(m_render_stage_linked->get_shader_program(), name), m_render_stage_linked->get_texture_count());

    if (connection_type == ConnectionType::INPUT) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_parameter_width, m_parameter_height, m_format, m_datatype, m_value);
    } else if (connection_type == ConnectionType::PASSTHROUGH) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_parameter_width, m_parameter_height, m_format, m_datatype, nullptr);
    }

}

void AudioTexture2DParameter::set_value(const void * value_ptr) {
    // Initialize m_value with enough memory
    m_value = new float[m_parameter_width * m_parameter_height];

    // Cast the value pointer to a float pointer
    const float* float_value_ptr = static_cast<const float*>(value_ptr);

    // Copy the value to m_value
    std::memcpy(m_value, float_value_ptr, m_parameter_width * m_parameter_height * sizeof(float));
}



