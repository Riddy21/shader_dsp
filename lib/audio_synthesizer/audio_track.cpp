#include <memory>

#include "audio_synthesizer/audio_track.h"

#include "audio_core/audio_render_stage.h"
#include "audio_render_stage/audio_effect_render_stage.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_render_stage/audio_file_generator_render_stage.h"

AudioTrack::AudioTrack(AudioRenderGraph * render_graph, AudioRenderStage * root_stage,
                       const unsigned int buffer_size,
                       const unsigned int sample_rate,
                       const unsigned int num_channels) :
    m_render_graph(render_graph),
    m_root_stage(root_stage),
    m_buffer_size(buffer_size),
    m_sample_rate(sample_rate),
    m_num_channels(num_channels) {

    // Check if the render graph is initialized
    if (!m_render_graph->is_initialized()) {
        std::cerr << "Render graph is not initialized." << std::endl;
        return;
    }

    // Create a dictionary of effects and generators with names
    initialize_effects();
    initialize_generators();

    // Set the default effect and generator
    m_current_effect_name = "frequency_filter";
    m_current_voice_name = "sine";
    m_current_effect = m_effects[m_current_effect_name].get();
    m_current_voice = m_generators[m_current_voice_name].get();

    // Set up the pipeline
    m_render_graph->insert_render_stage_infront(m_root_stage->gid, std::move(m_generators[m_current_voice_name]));
    m_render_graph->insert_render_stage_infront(m_root_stage->gid, std::move(m_effects[m_current_effect_name]));

    // remove those from the current effect list
    m_effects.erase(m_current_effect_name);
    m_generators.erase(m_current_voice_name);

}

void AudioTrack::initialize_effects() {
    m_effects["gain"] = std::make_unique<AudioGainEffectRenderStage>(m_buffer_size, m_sample_rate, m_num_channels);
    m_effects["echo"] = std::make_unique<AudioEchoEffectRenderStage>(m_buffer_size, m_sample_rate, m_num_channels);
    m_effects["frequency_filter"] = std::make_unique<AudioFrequencyFilterEffectRenderStage>(m_buffer_size, m_sample_rate, m_num_channels);
    m_effects["none"] = std::make_unique<AudioEffectRenderStage>(m_buffer_size, m_sample_rate, m_num_channels);
}

void AudioTrack::initialize_generators() {
    m_generators["sine"] = std::make_unique<AudioGeneratorRenderStage>(m_buffer_size, m_sample_rate, m_num_channels, "build/shaders/multinote_sine_generator_render_stage.glsl");
    m_generators["saw"] = std::make_unique<AudioGeneratorRenderStage>(m_buffer_size, m_sample_rate, m_num_channels, "build/shaders/multinote_sawtooth_generator_render_stage.glsl");
    m_generators["square"] = std::make_unique<AudioGeneratorRenderStage>(m_buffer_size, m_sample_rate, m_num_channels, "build/shaders/multinote_square_generator_render_stage.glsl");
    m_generators["triangle"] = std::make_unique<AudioGeneratorRenderStage>(m_buffer_size, m_sample_rate, m_num_channels, "build/shaders/multinote_triangle_generator_render_stage.glsl");
    m_generators["file"] = std::make_unique<AudioFileGeneratorRenderStage>(m_buffer_size, m_sample_rate, m_num_channels, "media/test.wav");
}

void AudioTrack::play_note(const float tone, const float gain) {
    auto generator = dynamic_cast<AudioGeneratorRenderStage *>(m_current_voice);
    if (generator) {
        generator->play_note(tone, gain);
    }
}

void AudioTrack::stop_note(const float tone) {
    auto generator = dynamic_cast<AudioGeneratorRenderStage *>(m_current_voice);
    if (generator) {
        generator->stop_note(tone);
    }
}

// FIXME: Fix modifying effect in the middle of a note
void AudioTrack::change_effect(const std::string & effect_name) {
    if (m_effects.find(effect_name) != m_effects.end()) {
        // Move the current effect back to the hash map
        AudioRenderGraph::GID gid = m_effects[effect_name]->gid;
        m_effects[m_current_effect_name] = std::move(m_render_graph->replace_render_stage(m_current_effect->gid, std::move(m_effects[effect_name])));
        m_effects.erase(effect_name);
        m_current_effect_name = effect_name;
        m_current_effect = m_render_graph->find_render_stage(gid);

        // Debugging log
        std::cout << "Switched effect to " << effect_name << std::endl;
    } else {
        std::cerr << "Effect not found: " << effect_name << std::endl;
    }
}

void AudioTrack::change_voice(const std::string & voice_name) {
    if (m_generators.find(voice_name) != m_generators.end()) {
        // Move the current generator back to the hash map
        AudioRenderGraph::GID gid = m_generators[voice_name]->gid;
        m_generators[m_current_voice_name] = std::move(m_render_graph->replace_render_stage(m_current_voice->gid, std::move(m_generators[voice_name])));
        m_generators.erase(voice_name);
        m_current_voice_name = voice_name;
        m_current_voice = m_render_graph->find_render_stage(gid);

        // Debugging log
        std::cout << "Switched voice to " << voice_name << std::endl;
    } else {
        std::cerr << "Voice not found: " << voice_name << std::endl;
    }
}

AudioTrack::~AudioTrack() {
    // Destructor implementation
}