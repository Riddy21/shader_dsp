#include <iostream>

#include "audio_render_stage/audio_final_render_stage.h"

AudioFinalRenderStage::AudioFinalRenderStage(const unsigned int frames_per_buffer,
                                             const unsigned int sample_rate,
                                             const unsigned int num_channels)
        : AudioRenderStage(frames_per_buffer, sample_rate, num_channels) {
}

void AudioFinalRenderStage::render_render_stage() {
    AudioRenderStage::render_render_stage();

    // Bind shader program
    glUseProgram(m_shader_program->get_program());
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

    glReadBuffer(GL_COLOR_ATTACHMENT0 + m_color_attachment_count);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PBO);

    glReadPixels(0, 0, m_frames_per_buffer * m_num_channels, 1, GL_RED, GL_FLOAT, 0);
    m_output_buffer_data = (float *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    // Display to screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Unbind everything
    glUseProgram(0);
}

bool AudioFinalRenderStage::initialize() {
    AudioRenderStage::initialize();

    // Generate the pixel buffer object
    glGenBuffers(1, &m_PBO);

    // Bind Pixel buffers
    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PBO);

    // Allocate memory for pixel buffer
    glBufferData(GL_PIXEL_PACK_BUFFER, m_frames_per_buffer * m_num_channels * sizeof(float), nullptr, GL_STREAM_READ);

    // Unbind the buffer
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "Failed to initialize OpenGL: " << error << std::endl;
        return false;
    }
    return true;
}