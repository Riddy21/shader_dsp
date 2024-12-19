#include <iostream>
#include <string>
#include "audio_render_stage/audio_multitrack_join_render_stage.h"
#include "audio_parameter/audio_texture2d_parameter.h"

const std::vector<std::string> AudioMultitrackJoinRenderStage::default_frag_shader_imports = {
    "build/shaders/global_settings.glsl",
    "build/shaders/frag_shader_settings.glsl"
};

AudioMultitrackJoinRenderStage::AudioMultitrackJoinRenderStage(const unsigned int frames_per_buffer,
                                                               const unsigned int sample_rate,
                                                               const unsigned int num_channels,
                                                               const unsigned int num_tracks,
                                                               const std::string& fragment_shader_path,
                                                               const std::vector<std::string> & frag_shader_imports)
    : AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {

    // NOTE: Pre-allocating 8 audio textures because cannot dynamically allocate in the shader
    //       You don't need to use all of them
    // Add parameters

    AudioParameter * input_audio_texture;
    for (unsigned int i=0; i<num_tracks; i++){
        std::string stream_name = "stream_audio_texture_" + std::to_string(i);

        input_audio_texture =
                new AudioTexture2DParameter(stream_name,
                                            AudioParameter::ConnectionType::PASSTHROUGH,
                                            m_frames_per_buffer * m_num_channels, 1,
                                            ++m_active_texture_count,
                                            0);

        // TODO: This needs to be a copy or else it doesn't work
        if (!this->add_parameter(input_audio_texture)) {
            std::cerr << "Failed to add " << stream_name << std::endl;
        }

        m_free_textures.push(this->find_parameter(stream_name.c_str()));
    }
}

AudioMultitrackJoinRenderStage::~AudioMultitrackJoinRenderStage() {}

AudioParameter * AudioMultitrackJoinRenderStage::get_free_stream_parameter() {
    AudioParameter * free_parameter = m_free_textures.front();

    // Pop from queue
    m_free_textures.pop();

    // Put in to use textures
    m_used_textures.insert(free_parameter);

    return free_parameter;
}
