#include <GL/glew.h>
#include <GL/glut.h>

#include "audio_render_stage_parameter.h"

GLuint AudioRenderStageParameter::color_attachment_index = GL_COLOR_ATTACHMENT0;

GLuint AudioRenderStageParameter::generate_framebuffer(const AudioRenderStageParameter & parameter) {
    GLuint framebuffer;
    glGenFramebuffers(1, &framebuffer);
    return framebuffer;
}

GLuint AudioRenderStageParameter::generate_texture(const AudioRenderStageParameter & parameter) {
    // Generate the texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    const float flat_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    // Configure the texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, flat_color);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    // allocate memory for the texture
    glTexImage2D(GL_TEXTURE_2D, 0, parameter.internal_format, parameter.parameter_width, parameter.parameter_height, 0, parameter.format, parameter.datatype, parameter.data);

    // Unbind the texture
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

void AudioRenderStageParameter::bind_texture_to_framebuffer(const AudioRenderStageParameter & output_parameter,
                                                                   const AudioRenderStageParameter & input_parameter) {
    // Bind the framebuffer and texture
    glBindFramebuffer(GL_FRAMEBUFFER, output_parameter.framebuffer);
    glBindTexture(GL_TEXTURE_2D, output_parameter.texture);

    // Configure the texture
    glFramebufferTexture2D(GL_FRAMEBUFFER, color_attachment_index, GL_TEXTURE_2D, output_parameter.texture, 0);

    // draw the buffer
    GLenum draw_buffers[1] = { color_attachment_index };
    glDrawBuffers(1, draw_buffers);
    
    // Reset the bindings
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Increment the color attachment index
    color_attachment_index++;
}