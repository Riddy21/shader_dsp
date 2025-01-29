#include <iostream>
#include <vector>

#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_parameter/audio_texture2d_parameter.h"
#include "audio_render_stage/audio_effect_render_stage.h"

const std::vector<std::string> AudioGainEffectRenderStage::default_frag_shader_imports = {
    "build/shaders/global_settings.glsl",
    "build/shaders/frag_shader_settings.glsl"
};

AudioGainEffectRenderStage::AudioGainEffectRenderStage(const unsigned int frames_per_buffer,
                                               const unsigned int sample_rate,
                                               const unsigned int num_channels,
                                               const std::string & fragment_shader_path,
                                               const std::vector<std::string> & frag_shader_imports)
    : AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {

    // Add new parameter objects to the parameter list
    auto gain_parameter =
        new AudioFloatParameter("gain",
                                AudioParameter::ConnectionType::INPUT);
    gain_parameter->set_value(1.0f);

    auto balance_parameter =
        new AudioFloatParameter("balance",
                                AudioParameter::ConnectionType::INPUT);
    balance_parameter->set_value(0.5f);

    if (!this->add_parameter(gain_parameter)) {
        std::cerr << "Failed to add gain_parameter" << std::endl;
    }
    if (!this->add_parameter(balance_parameter)) {
        std::cerr << "Failed to add balance_parameter" << std::endl;
    }
}

const std::vector<std::string> AudioEchoEffectRenderStage::default_frag_shader_imports = {
    "build/shaders/global_settings.glsl",
    "build/shaders/frag_shader_settings.glsl"
};

AudioEchoEffectRenderStage::AudioEchoEffectRenderStage(const unsigned int frames_per_buffer,
                                               const unsigned int sample_rate,
                                               const unsigned int num_channels,
                                               const std::string & fragment_shader_path,
                                               const std::vector<std::string> & frag_shader_imports)
    : AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {

    // Add new parameter objects to the parameter list
    auto feedback_parameter =
        new AudioIntParameter("num_echos",
                                AudioParameter::ConnectionType::INPUT);
    feedback_parameter->set_value(5);

    auto delay_parameter =
        new AudioFloatParameter("delay",
                                AudioParameter::ConnectionType::INPUT);
    delay_parameter->set_value(0.5f);

    auto decay_parameter =
        new AudioFloatParameter("decay",
                                AudioParameter::ConnectionType::INPUT);
    decay_parameter->set_value(0.5f);

    auto echo_audio_texture =
        new AudioTexture2DParameter("echo_audio_texture",
                                AudioParameter::ConnectionType::INPUT,
                                frames_per_buffer * num_channels, M_MAX_ECHO_BUFFER_SIZE * 2, // Around 2s of audio data
                                ++m_active_texture_count,
                                0);

    // Set the echo buffer to the size of the audio data and set to 0
    m_echo_buffer.resize(frames_per_buffer * num_channels * M_MAX_ECHO_BUFFER_SIZE * 2, 0.0f);

    // Set to 0 to start
    echo_audio_texture->set_value(m_echo_buffer.data());

    if (!this->add_parameter(feedback_parameter)) {
        std::cerr << "Failed to add feedback_parameter" << std::endl;
    }
    if (!this->add_parameter(delay_parameter)) {
        std::cerr << "Failed to add delay_parameter" << std::endl;
    }
    if (!this->add_parameter(decay_parameter)) {
        std::cerr << "Failed to add decay_parameter" << std::endl;
    }
    if (!this->add_parameter(echo_audio_texture)) {
        std::cerr << "Failed to add echo_audio_texture" << std::endl;
    }

}

void AudioEchoEffectRenderStage::render(unsigned int time) {

    // Get the audio data
    auto * data = (float *)this->find_parameter("stream_audio_texture")->get_value();

    if (time != m_time) {
        // shift the echo buffer
        std::copy(m_echo_buffer.begin(), m_echo_buffer.end() - m_frames_per_buffer * m_num_channels * 2, m_echo_buffer.begin() + m_frames_per_buffer * m_num_channels * 2);

        // Add the new data to the echo buffer
        std::copy(data, data + m_frames_per_buffer * m_num_channels, m_echo_buffer.begin());

        // Insert a row of 0s to m_echo_buffer righ after the new data
        std::fill(m_echo_buffer.begin() + m_frames_per_buffer * m_num_channels, m_echo_buffer.begin() + m_frames_per_buffer * m_num_channels * 2, 0.0f);

        this->find_parameter("echo_audio_texture")->set_value(m_echo_buffer.data());
    }

    AudioRenderStage::render(time);

}