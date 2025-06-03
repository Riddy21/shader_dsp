#ifndef AUDIO_TRACK_H
#define AUDIO_TRACK_H

#include <memory>
#include <unordered_map>
#include <string>

#include "audio_core/audio_render_graph.h"
#include "audio_render_stage/audio_effect_render_stage.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_synthesizer/audio_module.h"
class AudioTrack {

public:
    AudioTrack(AudioRenderGraph * render_graph, AudioRenderStage * root_stage,
               const unsigned int buffer_size,
               const unsigned int sample_rate,
               const unsigned int num_channels);
    ~AudioTrack();

    // TODO: Move these to the voice track stage
    void play_note(const float tone, const float gain);
    void stop_note(const float tone);

    void change_effect(const std::string & effect_name);
    void change_voice(const std::string & voice_name);

    const std::unordered_map<std::string, std::shared_ptr<AudioEffectModule>> & get_effects() const;
    const std::vector<std::string> get_effect_names() const;
    const std::unordered_map<std::string, std::shared_ptr<AudioVoiceModule>> & get_voices() const;
    const std::vector<std::string> get_generator_names() const;

private:
    void initialize_modules();

    const unsigned int m_buffer_size;
    const unsigned int m_sample_rate;
    const unsigned int m_num_channels;

    AudioRenderGraph * m_render_graph;
    AudioRenderStage * m_root_stage;
    AudioRenderer * m_audio_renderer;

    AudioModuleManager m_module_manager;

    std::shared_ptr<AudioEffectModule> m_current_effect;
    std::shared_ptr<AudioVoiceModule> m_current_voice;
    std::unordered_map<std::string, std::shared_ptr<AudioEffectModule>> m_effect_modules;
    std::unordered_map<std::string, std::shared_ptr<AudioVoiceModule>> m_voice_modules;

};

#endif // AUDIO_TRACK_H