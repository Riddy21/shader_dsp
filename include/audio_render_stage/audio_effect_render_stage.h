#pragma once
#ifndef AUDIO_EFFECT_RENDER_STAGE_H
#define AUDIO_EFFECT_RENDER_STAGE_H

#include "audio_core/audio_render_stage.h"
#include "audio_render_stage/audio_render_stage_history.h"
#include "audio_core/audio_control.h"

class AudioEffectRenderStage : public AudioRenderStage {
public:
    AudioEffectRenderStage(const unsigned int frames_per_buffer,
                           const unsigned int sample_rate,
                           const unsigned int num_channels,
                           const std::string& fragment_shader_path = "build/shaders/render_stage_frag.glsl",
                           const std::vector<std::string> & frag_shader_imports = AudioRenderStage::default_frag_shader_imports) : 
        AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {
    }

    ~AudioEffectRenderStage() {};
};

class AudioGainEffectRenderStage : public AudioEffectRenderStage {
public:
    AudioGainEffectRenderStage(const unsigned int frames_per_buffer,
                           const unsigned int sample_rate,
                           const unsigned int num_channels,
                           const std::string& fragment_shader_path = "build/shaders/gain_effect_render_stage.glsl",
                           const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports);

    static const std::vector<std::string> default_frag_shader_imports;

    ~AudioGainEffectRenderStage() {};

private:
    std::vector<AudioControl<float>*> m_controls;
};

class AudioEchoEffectRenderStage : public AudioEffectRenderStage {
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

    bool disconnect_render_stage(AudioRenderStage * render_stage) override;

    std::vector<float> m_echo_buffer;

    static const unsigned int M_MAX_ECHO_BUFFER_SIZE = 300;
    std::vector<AudioControl<float>*> m_controls;
};

class AudioFrequencyFilterEffectRenderStage : public AudioEffectRenderStage {
public:
    AudioFrequencyFilterEffectRenderStage(const unsigned int frames_per_buffer,
                           const unsigned int sample_rate,
                           const unsigned int num_channels,
                           const std::string& fragment_shader_path = "build/shaders/frequency_filter_effect_render_stage.glsl",
                           const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports);

    static const std::vector<std::string> default_frag_shader_imports;

    void set_low_pass(const float low_pass) { m_low_pass = low_pass; update_b_coefficients(); };
    void set_high_pass(const float high_pass) { m_high_pass = high_pass; update_b_coefficients(); };
    void set_filter_follower(const float filter_follower) { m_filter_follower = filter_follower; };
    void set_resonance(const float resonance) { m_resonance = resonance; update_b_coefficients(); };

    const float get_low_pass() { return m_low_pass; };
    const float get_high_pass() { return m_high_pass; };
    const float get_filter_follower() { return m_filter_follower; };
    const float get_resonance() { return m_resonance; };

    ~AudioFrequencyFilterEffectRenderStage() {};

private:
    static const std::vector<float> calculate_firwin_b_coefficients(const float low_pass, const float high_pass, const unsigned int num_taps, const float resonance);
    void update_b_coefficients(const float current_amplitude = 0.0);
    void render(const unsigned int time) override;
    bool disconnect_render_stage(AudioRenderStage * render_stage) override;

    std::unique_ptr<AudioRenderStageHistory> m_audio_history;
    float m_low_pass;
    float m_high_pass;
    float m_filter_follower;
    float m_resonance;
    const float NYQUIST;

    std::vector<AudioControl<float>*> m_controls;
};

#endif // AUDIO_EFFECT_RENDER_STAGE_H