#pragma once
#ifndef AUDIO_EFFECT_RENDER_STAGE_H
#define AUDIO_EFFECT_RENDER_STAGE_H

#include "audio_render_stage/audio_render_stage.h"

class AudioEffectRenderStage : public AudioRenderStage {
public:
    AudioEffectRenderStage(const unsigned int frames_per_buffer,
                           const unsigned int sample_rate,
                           const unsigned int num_channels,
                           const std::string& fragment_shader_path,
                           const std::vector<std::string> & frag_shader_imports) : 
        AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {
    }

    ~AudioEffectRenderStage() {};
};

class AudioGainEffectRenderStage : public AudioRenderStage {
public:
    AudioGainEffectRenderStage(const unsigned int frames_per_buffer,
                           const unsigned int sample_rate,
                           const unsigned int num_channels,
                           const std::string& fragment_shader_path = "build/shaders/gain_effect_render_stage.glsl",
                           const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports);

    static const std::vector<std::string> default_frag_shader_imports;

    ~AudioGainEffectRenderStage() {};
};

class AudioEchoEffectRenderStage : public AudioRenderStage {
public:
    AudioEchoEffectRenderStage(const unsigned int frames_per_buffer,
                           const unsigned int sample_rate,
                           const unsigned int num_channels,
                           const std::string& fragment_shader_path = "build/shaders/echo_effect_render_stage.glsl",
                           const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports);

    static const std::vector<std::string> default_frag_shader_imports;

    ~AudioEchoEffectRenderStage() {};

private:
    void render(const unsigned int time) override;

    std::vector<float> m_echo_buffer;

    static const unsigned int M_MAX_ECHO_BUFFER_SIZE = 300;
};

class AudioFrequencyFilterEffectRenderStage : public AudioRenderStage {
public:
    AudioFrequencyFilterEffectRenderStage(const unsigned int frames_per_buffer,
                           const unsigned int sample_rate,
                           const unsigned int num_channels,
                           const std::string& fragment_shader_path = "build/shaders/frequency_filter_effect_render_stage.glsl",
                           const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports);

    static const std::vector<std::string> default_frag_shader_imports;

    ~AudioFrequencyFilterEffectRenderStage() {};
};

#endif // AUDIO_EFFECT_RENDER_STAGE_H