#pragma once
#ifndef AUDIO_EFFECT_RENDER_STAGE_H
#define AUDIO_EFFECT_RENDER_STAGE_H

#include "audio_render_stage.h"

class AudioEffectRenderStage : public AudioRenderStage {
public:
    AudioEffectRenderStage(const unsigned int frames_per_buffer,
                           const unsigned int sample_rate,
                           const unsigned int num_channels,
                           const std::string& fragment_shader_path = "build/shaders/render_stage_frag.glsl",
                           const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports);

    static const std::vector<std::string> default_frag_shader_imports;

    ~AudioEffectRenderStage();
};

#endif // AUDIO_EFFECT_RENDER_STAGE_H