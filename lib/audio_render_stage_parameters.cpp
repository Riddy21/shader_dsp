#include <GL/glew.h>
#include <GL/glut.h>
#include <iostream>

#include "audio_render_stage_parameter.h"

GLuint AudioRenderStageParameter::color_attachment_index = GL_COLOR_ATTACHMENT0;

GLuint AudioRenderStageParameter::generate_texture(AudioRenderStageParameter & parameter) {
    // Generate the texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

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
    glTexImage2D(GL_TEXTURE_2D, 0, parameter.internal_format, parameter.parameter_width, parameter.parameter_height, 0, parameter.format, parameter.datatype, nullptr);

    // Unbind the texture
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

void AudioRenderStageParameter::bind_framebuffer_to_texture(AudioRenderStageParameter & output_parameter,
                                                            AudioRenderStageParameter & input_parameter) {
    // Bind the framebuffer and texture
    glBindFramebuffer(GL_FRAMEBUFFER, output_parameter.framebuffer);
    glBindTexture(GL_TEXTURE_2D, input_parameter.texture);

    // Configure the texture
    glFramebufferTexture2D(GL_FRAMEBUFFER, color_attachment_index, GL_TEXTURE_2D, input_parameter.texture, 0);
    printf("Color attachment index: %d\n", color_attachment_index);

    // draw the buffer
    GLenum draw_buffers[1] = { color_attachment_index };
    glDrawBuffers(1, draw_buffers);
    
    // Reset the bindings
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Increment the color attachment index
    color_attachment_index++;

    GLint maxColorAttachments;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);
    if (GL_COLOR_ATTACHMENT0 + (GLuint)maxColorAttachments < color_attachment_index) {
        color_attachment_index = GL_COLOR_ATTACHMENT0;
    }

    output_parameter.is_bound = true;
    input_parameter.is_bound = true;
}