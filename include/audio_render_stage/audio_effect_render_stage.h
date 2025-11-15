#pragma once
#ifndef AUDIO_EFFECT_RENDER_STAGE_H
#define AUDIO_EFFECT_RENDER_STAGE_H

#include "audio_core/audio_render_stage.h"
#include "audio_render_stage_plugins/audio_render_stage_history.h"
#include "audio_core/audio_tape.h"
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

    // Named constructor
    AudioEffectRenderStage(const std::string & stage_name,
                           const unsigned int frames_per_buffer,
                           const unsigned int sample_rate,
                           const unsigned int num_channels,
                           const std::string& fragment_shader_path = "build/shaders/render_stage_frag.glsl",
                           const std::vector<std::string> & frag_shader_imports = AudioRenderStage::default_frag_shader_imports) : 
        AudioRenderStage(stage_name, frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {
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

    // Named constructor
    AudioGainEffectRenderStage(const std::string & stage_name,
                            const unsigned int frames_per_buffer,
                            const unsigned int sample_rate,
                            const unsigned int num_channels,
                            const std::string& fragment_shader_path = "build/shaders/gain_effect_render_stage.glsl",
                            const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports);

    static const std::vector<std::string> default_frag_shader_imports;

    // Convenience function to set gains for actual number of channels
    void set_channel_gains(const std::vector<float>& channel_gains);

    ~AudioGainEffectRenderStage() {};
};

class AudioEchoEffectRenderStage : public AudioEffectRenderStage {
public:
    AudioEchoEffectRenderStage(const unsigned int frames_per_buffer,
                           const unsigned int sample_rate,
                           const unsigned int num_channels,
                           const std::string& fragment_shader_path = "build/shaders/echo_effect_render_stage.glsl",
                           const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports);

    // Named constructor
    AudioEchoEffectRenderStage(const std::string & stage_name,
                           const unsigned int frames_per_buffer,
                           const unsigned int sample_rate,
                           const unsigned int num_channels,
                           const std::string& fragment_shader_path = "build/shaders/echo_effect_render_stage.glsl",
                           const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports);

    static const std::vector<std::string> default_frag_shader_imports;

    ~AudioEchoEffectRenderStage() {};

private:
    static constexpr unsigned int M_MAX_ECHO_BUFFER_SIZE = 500;

    void render(const unsigned int time) override;

    bool disconnect_render_stage(AudioRenderStage * render_stage) override;

    std::vector<float> m_echo_buffer;
};

class AudioFrequencyFilterEffectRenderStage : public AudioEffectRenderStage {
public:
    AudioFrequencyFilterEffectRenderStage(const unsigned int frames_per_buffer,
                           const unsigned int sample_rate,
                           const unsigned int num_channels,
                           const std::string& fragment_shader_path = "build/shaders/frequency_filter_effect_render_stage.glsl",
                           const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports);

    // Named constructor
    AudioFrequencyFilterEffectRenderStage(const std::string & stage_name,
                           const unsigned int frames_per_buffer,
                           const unsigned int sample_rate,
                           const unsigned int num_channels,
                           const std::string& fragment_shader_path = "build/shaders/frequency_filter_effect_render_stage.glsl",
                           const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports);

    static const std::vector<std::string> default_frag_shader_imports;

    void set_low_pass(const float low_pass) { m_low_pass = low_pass; m_b_coefficients_dirty = true; };
    void set_high_pass(const float high_pass) { m_high_pass = high_pass; m_b_coefficients_dirty = true; };
    void set_filter_follower(const float filter_follower) { m_filter_follower = filter_follower; };
    void set_resonance(const float resonance) { m_resonance = resonance; m_b_coefficients_dirty = true; };

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
    std::unique_ptr<AudioRenderStageHistory2> m_history2;
    std::shared_ptr<AudioTape> m_tape;
    float m_low_pass;
    float m_high_pass;
    float m_filter_follower;
    float m_resonance;
    const float NYQUIST;

    bool m_b_coefficients_dirty = true;
};

#endif // AUDIO_EFFECT_RENDER_STAGE_H