#pragma once
#ifndef AUDIO_EFFECT_RENDER_STAGE_H
#define AUDIO_EFFECT_RENDER_STAGE_H

#include "audio_render_stage.h"

class AudioEffectRenderStage : public AudioRenderStage {
public:
    AudioEffectRenderStage(const unsigned int frames_per_buffer,
                           const unsigned int sample_rate,
                           const unsigned int num_channels,
                           const std::string& fragment_shader_path = "build/shaders/effect_stage.frag",
                           const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports,
                           const std::string& vertex_shader_path = "build/shaders/effect_stage.vert",
                           const std::vector<std::string> & vert_shader_imports = default_vert_shader_imports);

    virtual ~AudioEffectRenderStage();

protected:
    virtual bool initialize_shader_stage() override;
    virtual bool bind_shader_stage() override;
    virtual void render_render_stage() override;

private:
    bool apply_effect();
};

#endif // AUDIO_EFFECT_RENDER_STAGE_H