#include <vector>

#include "audio_render_stage/audio_effect_render_stage.h"

const std::vector<std::string> AudioEffectRenderStage::default_frag_shader_imports = {
    "build/shaders/global_settings.glsl",
    "build/shaders/frag_shader_settings.glsl"
};

AudioEffectRenderStage::AudioEffectRenderStage(const unsigned int frames_per_buffer,
                                               const unsigned int sample_rate,
                                               const unsigned int num_channels,
                                               const std::string & fragment_shader_path,
                                               const std::vector<std::string> & frag_shader_imports)
    : AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {

    // Add new parameter objects to the parameter list
}

AudioEffectRenderStage::~AudioEffectRenderStage() {}