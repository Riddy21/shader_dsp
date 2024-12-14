#include <iostream>
#include "audio_render_stage/audio_multitrack_join_render_stage.h"
#include "audio_parameter/audio_texture2d_parameter.h"

const std::vector<std::string> AudioMultitrackJoinRenderStage::default_frag_shader_imports = {
    "build/shaders/global_settings.glsl",
    "build/shaders/frag_shader_settings.glsl"
};

AudioMultitrackJoinRenderStage::AudioMultitrackJoinRenderStage(const unsigned int frames_per_buffer,
                                                               const unsigned int sample_rate,
                                                               const unsigned int num_channels,
                                                               const std::string& fragment_shader_path,
                                                               const std::vector<std::string> & frag_shader_imports)
    : AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {

    // NOTE: Pre-allocating 8 audio textures because cannot dynamically allocate in the shader
    //       You don't need to use all of them
    // Add parameters
    auto input_audio_texture_1 =
        new AudioTexture2DParameter("stream_audio_texture_1",
                                    AudioParameter::ConnectionType::PASSTHROUGH,
                                    m_frames_per_buffer * m_num_channels, 1,
                                    ++m_active_texture_count,
                                    0);

    auto input_audio_texture_2 =
        new AudioTexture2DParameter("stream_audio_texture_2",
                                    AudioParameter::ConnectionType::PASSTHROUGH,
                                    m_frames_per_buffer * m_num_channels, 1,
                                    ++m_active_texture_count,
                                    0);

    auto input_audio_texture_3 =
        new AudioTexture2DParameter("stream_audio_texture_3",
                                    AudioParameter::ConnectionType::PASSTHROUGH,
                                    m_frames_per_buffer * m_num_channels, 1,
                                    ++m_active_texture_count,
                                    0);
    auto input_audio_texture_4 =
        new AudioTexture2DParameter("stream_audio_texture_4",
                                    AudioParameter::ConnectionType::PASSTHROUGH,
                                    m_frames_per_buffer * m_num_channels, 1,
                                    ++m_active_texture_count,
                                    0);

    auto input_audio_texture_5 =
        new AudioTexture2DParameter("stream_audio_texture_5",
                                    AudioParameter::ConnectionType::PASSTHROUGH,
                                    m_frames_per_buffer * m_num_channels, 1,
                                    ++m_active_texture_count,
                                    0);

    auto input_audio_texture_6 =
        new AudioTexture2DParameter("stream_audio_texture_6",
                                    AudioParameter::ConnectionType::PASSTHROUGH,
                                    m_frames_per_buffer * m_num_channels, 1,
                                    ++m_active_texture_count,
                                    0);
    auto input_audio_texture_7 =
        new AudioTexture2DParameter("stream_audio_texture_7",
                                    AudioParameter::ConnectionType::PASSTHROUGH,
                                    m_frames_per_buffer * m_num_channels, 1,
                                    ++m_active_texture_count,
                                    0);

    auto input_audio_texture_8 =
        new AudioTexture2DParameter("stream_audio_texture_8",
                                    AudioParameter::ConnectionType::PASSTHROUGH,
                                    m_frames_per_buffer * m_num_channels, 1,
                                    ++m_active_texture_count,
                                    0);
    

    if (!this->add_parameter(input_audio_texture_1)) {
        std::cerr << "Failed to add input_audio_texture_1" << std::endl;
    }
    if (!this->add_parameter(input_audio_texture_2)) {
        std::cerr << "Failed to add input_audio_texture_2" << std::endl;
    }
    if (!this->add_parameter(input_audio_texture_3)) {
        std::cerr << "Failed to add input_audio_texture_3" << std::endl;
    }
    if (!this->add_parameter(input_audio_texture_4)) {
        std::cerr << "Failed to add input_audio_texture_4" << std::endl;
    }
    if (!this->add_parameter(input_audio_texture_5)) {
        std::cerr << "Failed to add input_audio_texture_5" << std::endl;
    }
    if (!this->add_parameter(input_audio_texture_6)) {
        std::cerr << "Failed to add input_audio_texture_6" << std::endl;
    }
    if (!this->add_parameter(input_audio_texture_7)) {
        std::cerr << "Failed to add input_audio_texture_7" << std::endl;
    }
    if (!this->add_parameter(input_audio_texture_8)) {
        std::cerr << "Failed to add input_audio_texture_8" << std::endl;
    }
}

AudioMultitrackJoinRenderStage::~AudioMultitrackJoinRenderStage() {}
