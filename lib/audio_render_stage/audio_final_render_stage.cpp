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
}

void AudioFinalRenderStage::render(unsigned int time) {
    // TODO: Shorten this so that it doesn't render anything and directly passes to next stage
    AudioRenderStage::render(time);

    // display to screen
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(m_shader_program->get_program());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glUseProgram(0);

    m_output_buffer_data = (float *)this->find_parameter("stream_audio_texture")->get_value();

     //Print out the sound 
    //for (int i = 0; i < m_frames_per_buffer * m_num_channels; i++) {
    //    printf("%f ", m_output_buffer_data[i]);
    //}

    //printf("\n");
    //printf("\n");
    //printf("\n");


}