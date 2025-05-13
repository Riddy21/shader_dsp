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
    // TODO: Make this data driven
    m_effects["gain"] = std::make_unique<AudioGainEffectRenderStage>(m_buffer_size, m_sample_rate, m_num_channels);
    m_effects["echo"] = std::make_unique<AudioEchoEffectRenderStage>(m_buffer_size, m_sample_rate, m_num_channels);
    m_effects["frequency_filter"] = std::make_unique<AudioFrequencyFilterEffectRenderStage>(m_buffer_size, m_sample_rate, m_num_channels);
    m_effects["none"] = std::make_unique<AudioEffectRenderStage>(m_buffer_size, m_sample_rate, m_num_channels);

    m_generators["sine"] = std::make_unique<AudioGeneratorRenderStage>(m_buffer_size, m_sample_rate, m_num_channels, "build/shaders/multinote_sine_generator_render_stage.glsl");
    m_generators["saw"] = std::make_unique<AudioGeneratorRenderStage>(m_buffer_size, m_sample_rate, m_num_channels, "build/shaders/multinote_sawtooth_generator_render_stage.glsl");
    m_generators["square"] = std::make_unique<AudioGeneratorRenderStage>(m_buffer_size, m_sample_rate, m_num_channels, "build/shaders/multinote_square_generator_render_stage.glsl");
    m_generators["triangle"] = std::make_unique<AudioGeneratorRenderStage>(m_buffer_size, m_sample_rate, m_num_channels, "build/shaders/multinote_triangle_generator_render_stage.glsl");
    m_generators["file"] = std::make_unique<AudioFileGeneratorRenderStage>(m_buffer_size, m_sample_rate, m_num_channels, "media/test.wav");

    // Set the default effect and generator
    m_current_effect = "echo";
    m_current_voice = "sine";

    // set up the pipeline,

    m_render_graph->insert_render_stage_infront(m_root_stage->gid, m_generators[m_current_voice].get());
    m_render_graph->insert_render_stage_infront(m_root_stage->gid, m_effects[m_current_effect].get());
}

void AudioTrack::play_note(const float tone, const float gain) {
    auto generator = dynamic_cast<AudioGeneratorRenderStage *>(m_generators[m_current_voice].get());
    if (generator) {
        generator->play_note(tone, gain);
    }
}

void AudioTrack::stop_note(const float tone) {
    auto generator = dynamic_cast<AudioGeneratorRenderStage *>(m_generators[m_current_voice].get());
    if (generator) {
        generator->stop_note(tone);
    }
}

AudioTrack::~AudioTrack() {
    // Destructor implementation
}