#include <iostream>

#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_parameter/audio_texture2d_parameter.h"

const std::vector<std::string> AudioFinalRenderStage::default_frag_shader_imports = {
    "build/shaders/global_settings.glsl",
    "build/shaders/frag_shader_settings.glsl"
};

AudioFinalRenderStage::AudioFinalRenderStage(const unsigned int frames_per_buffer,
                                             const unsigned int sample_rate,
                                             const unsigned int num_channels,
                                             const std::string& fragment_shader_path,
                                             const std::vector<std::string> & frag_shader_imports)
        : AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {

    //auto final_output_texture =
    //    new AudioTexture2DParameter("final_output_texture",
    //                                AudioParameter::ConnectionType::OUTPUT,
    //                                100, 1,
    //                                0,
    //                                ++m_color_attachment_count);

    //if (!this->add_parameter(final_output_texture)) {
    //    std::cerr << "Failed to add output_audio_texture" << std::endl;
    //}
}

void AudioFinalRenderStage::render() {
    // TODO: Shorten this so that it doesn't render anything and directly passes to next stage
    AudioRenderStage::render();

    //auto * data = this->find_parameter("final_output_texture")->get_value();
    //for (unsigned int i = 0; i < 100; i++) {
    //    std::cout << ((float *)data)[i] << " ";
    //}
    //printf("\n\n");

    //auto * output = this->find_parameter("output_audio_texture")->get_value();
    //for (unsigned int i = 0; i < m_frames_per_buffer * m_num_channels; i++) {
    //    std::cout << ((float *)output)[i] << " ";
    //}
    //printf("\n\n");


    // Bind shader program
    glUseProgram(m_shader_program->get_program());
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

    glReadBuffer(GL_COLOR_ATTACHMENT0 + m_color_attachment_count);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PBO);

    AudioTexture2DParameter * stream_texture = dynamic_cast<AudioTexture2DParameter *>(this->find_parameter("output_audio_texture"));
    stream_texture->transfer_texture_data_to_buffer();

    m_output_buffer_data = (float *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    // Display to screen
    //glBindFramebuffer(GL_FRAMEBUFFER, 0);
    //glClear(GL_COLOR_BUFFER_BIT);
    //glDrawArrays(GL_TRIANGLES, 0, 6);

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
    glBufferData(GL_PIXEL_PACK_BUFFER, (m_frames_per_buffer * m_num_channels) * sizeof(float), nullptr, GL_STREAM_READ);

    // Unbind the buffer
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "Failed to initialize OpenGL: " << error << std::endl;
        return false;
    }
    return true;
}