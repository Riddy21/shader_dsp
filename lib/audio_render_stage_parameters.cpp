#include <GL/glew.h>
#include <GL/glut.h>
#include <iostream>

#include "audio_render_stage_parameter.h"

GLuint AudioRenderStageParameter::m_color_attachment_index = GL_COLOR_ATTACHMENT0;

void AudioRenderStageParameter::generate_texture() {
    // Generate the texture
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);

    const float flat_color[] = { 0.0f, 0.0f, 0.0f, 0.0f };

    // Configure the texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, flat_color);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    // allocate memory for the texture
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, parameter_width, parameter_height, 0, format, datatype, nullptr);

    // Unbind the texture
    glBindTexture(GL_TEXTURE_2D, 0);
}

void AudioRenderStageParameter::bind_framebuffer_to_texture(AudioRenderStageParameter & output_parameter,
                                                            AudioRenderStageParameter & input_parameter) {

    // Increment the color attachment index
    m_color_attachment_index++;

    GLint maxColorAttachments;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);
    if (GL_COLOR_ATTACHMENT0 + (GLuint)maxColorAttachments < m_color_attachment_index) {
        m_color_attachment_index = GL_COLOR_ATTACHMENT0;
    }

    // Bind the framebuffer and texture
    glBindFramebuffer(GL_FRAMEBUFFER, output_parameter.get_framebuffer());
    glBindTexture(GL_TEXTURE_2D, input_parameter.get_texture());

    // Configure the texture
    glFramebufferTexture2D(GL_FRAMEBUFFER, m_color_attachment_index, GL_TEXTURE_2D, input_parameter.get_texture(), 0);
    printf("Color attachment index: %d\n", m_color_attachment_index);

    // draw the buffer
    GLenum draw_buffers[1] = { m_color_attachment_index };
    glDrawBuffers(1, draw_buffers);
    
    // Reset the bindings
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    output_parameter.m_is_bound = true;
    input_parameter.m_is_bound = true;
}