#include <iostream>
#include <vector>

#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_render_stage/audio_gain_effect_render_stage.h"

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

AudioGainEffectRenderStage::~AudioGainEffectRenderStage() {}