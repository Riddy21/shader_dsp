#ifndef AUDIO_TRACK_H
#define AUDIO_TRACK_H

#include <memory>
#include <unordered_map>
#include <string>


#include "audio_core/audio_render_graph.h"
#include "audio_render_stage/audio_effect_render_stage.h"
#include "audio_render_stage/audio_generator_render_stage.h"

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

private:
    const unsigned int m_buffer_size;
    const unsigned int m_sample_rate;
    const unsigned int m_num_channels;

    // TODO: Change these all to components
    AudioRenderGraph * m_render_graph;
    AudioRenderStage * m_root_stage;
    std::string m_current_effect;
    std::string m_current_voice;

    std::unordered_map<std::string, std::unique_ptr<AudioEffectRenderStage>> m_effects;
    std::unordered_map<std::string, std::unique_ptr<AudioGeneratorRenderStage>> m_generators;
};


#endif // AUDIO_TRACK_H