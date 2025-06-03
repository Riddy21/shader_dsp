#include <iostream>
#include <string>
#include "audio_render_stage/audio_multitrack_join_render_stage.h"
#include "audio_parameter/audio_texture2d_parameter.h"

#define MAX_TRACKS 9

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

    if (num_tracks > MAX_TRACKS) {
        std::cerr << "Cannot add more than " << MAX_TRACKS << " tracks" << std::endl;
    }

    for (unsigned int i=0; i<num_tracks; i++){
        std::string stream_name = "stream_audio_texture_" + std::to_string(i);

        AudioParameter * input_audio_texture = 
                new AudioTexture2DParameter(stream_name,
                                            AudioParameter::ConnectionType::PASSTHROUGH,
                                            frames_per_buffer, num_channels,
                                            ++m_active_texture_count,
                                            0, GL_NEAREST);

        if (!this->add_parameter(input_audio_texture)) {
            std::cerr << "Failed to add " << stream_name << std::endl;
        }

        m_free_textures.push(input_audio_texture);
    }
}

const std::vector<AudioParameter *> AudioMultitrackJoinRenderStage::get_stream_interface() {
    AudioParameter * free_parameter = m_free_textures.front();

    // Pop from queue
    m_free_textures.pop();

    // Put in to use textures
    m_used_textures.insert(free_parameter);

    return {free_parameter};
}

bool AudioMultitrackJoinRenderStage::release_stream_interface(AudioRenderStage * prev_stage){
    AudioRenderStage::release_stream_interface(prev_stage);

    // Find linked render stage stream parameter
    auto * parameter = prev_stage->find_parameter("output_audio_texture")->get_linked_parameter();

    parameter->clear_value();

    // Find audio parameter
    if (m_used_textures.find(parameter) == m_used_textures.end()){
        printf("Parameter %s is not used\n", parameter->name.c_str());
        return false;
    }

    // Erase it if its there
    m_used_textures.erase(parameter);
    // Push to free queue
    m_free_textures.push(parameter);

    return true;
}