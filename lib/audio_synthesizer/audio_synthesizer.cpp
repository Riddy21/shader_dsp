#include "audio_synthesizer/audio_synthesizer.h"

#include "audio_core/audio_renderer.h"
#include "audio_core/audio_render_graph.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_render_stage/audio_tape_render_stage.h"
#include "audio_render_stage/audio_effect_render_stage.h"
#include "audio_output/audio_player_output.h"
#include "engine/event_loop.h"


// Initialize the static instance pointer
AudioSynthesizer* AudioSynthesizer::instance = nullptr;

AudioSynthesizer::AudioSynthesizer() :
        m_audio_renderer(AudioRenderer::get_instance()) {

    // Add the Renderer to the event loop
    auto& event_loop = EventLoop::get_instance();
    event_loop.add_loop_item(&m_audio_renderer);
}

AudioSynthesizer::~AudioSynthesizer() {
    delete instance;
}

bool AudioSynthesizer::initialize(const unsigned int buffer_size,
                                 const unsigned int sample_rate,
                                 const unsigned int num_channels) {
    m_buffer_size = buffer_size;
    m_sample_rate = sample_rate;
    m_num_channels = num_channels;

    // Create the render stages
    //auto first_render_stage = new AudioRenderStage(m_buffer_size, m_sample_rate, m_num_channels);
    m_render_stages["final"] = new AudioFinalRenderStage(m_buffer_size, m_sample_rate, m_num_channels);

    // Initialize the render graph
    m_render_graph = new AudioRenderGraph(m_render_stages["final"]);

    if (!m_audio_renderer.add_render_graph(m_render_graph)) {
        std::cerr << "Failed to add render graph to audio renderer." << std::endl;
        return false;
    }

    // Initialize basic render output
    auto audio_player_output = new AudioPlayerOutput(m_buffer_size, m_sample_rate, m_num_channels);
    m_render_outputs.push_back(audio_player_output);

    if (!m_audio_renderer.add_render_output(audio_player_output)) {
        std::cerr << "Failed to add render output to audio renderer." << std::endl;
        return false;
    }

    // Initialize the renderer
    m_audio_renderer.initialize(m_buffer_size, m_sample_rate, m_num_channels);

    m_audio_renderer.set_lead_output(0);

    // FIXME: Delete this after testing
    m_audio_generator = new AudioGeneratorRenderStage(m_buffer_size, m_sample_rate, m_num_channels, "build/shaders/multinote_sine_generator_render_stage.glsl");
    m_render_graph->insert_render_stage_infront(m_render_stages["final"]->gid, m_audio_generator);

    auto audio_echo_effect = new AudioEchoEffectRenderStage(m_buffer_size, m_sample_rate, m_num_channels);
    m_render_graph->insert_render_stage_infront(m_render_stages["final"]->gid, audio_echo_effect);

    auto audio_filter_effect = new AudioFrequencyFilterEffectRenderStage(m_buffer_size, m_sample_rate, m_num_channels);
    m_render_graph->insert_render_stage_infront(m_render_stages["final"]->gid, audio_filter_effect);

    m_audio_recorder = new AudioRecordRenderStage(m_buffer_size, m_sample_rate, m_num_channels);
    m_render_graph->insert_render_stage_infront(m_render_stages["final"]->gid, m_audio_recorder);

    m_audio_playback = new AudioPlaybackRenderStage(m_buffer_size, m_sample_rate, m_num_channels);
    m_render_graph->insert_render_stage_infront(m_render_stages["final"]->gid, m_audio_playback);

    return true;
}

// FIXME: Delete this after testing / or modify to use engine object
void AudioSynthesizer::play_note(const float tone, const float volume) {
    m_audio_generator->play_note(tone, volume);
}

// FIXME: Delete this after testing / or modify to use engine object
void AudioSynthesizer::stop_note(const float tone) {
    m_audio_generator->stop_note(tone);
}

bool AudioSynthesizer::start() {
    // Start the audio outputs
    for (auto& output : m_render_outputs) {
        if (!output->open()) {
            std::cerr << "Failed to open audio output." << std::endl;
            return false;
        }
        if (!output->start()) {
            std::cerr << "Failed to start audio output." << std::endl;
            return false;
        }
    }

    return true;
}

bool AudioSynthesizer::pause() {
    m_audio_renderer.pause();
    
    // Stop the audio outputs
    for (auto& output : m_render_outputs) {
        if (!output->stop()) {
            std::cerr << "Failed to stop audio output." << std::endl;
            return false;
        }
    }
    return true;
}

bool AudioSynthesizer::increment() {
    m_audio_renderer.increment();

    return true;
}

bool AudioSynthesizer::resume() {
    // Start the audio outputs
    for (auto& output : m_render_outputs) {
        if (!output->start()) {
            std::cerr << "Failed to start audio output." << std::endl;
            return false;
        }
    }

    m_audio_renderer.resume();

    return true;
}

bool AudioSynthesizer::close() {
    // Stop the audio outputs
    for (auto& output : m_render_outputs) {
        if (!output->stop()) {
            std::cerr << "Failed to stop audio output." << std::endl;
            return false;
        }
        if (!output->close()) {
            std::cerr << "Failed to close audio output." << std::endl;
            return false;
        }
    }
    return true;
}

bool AudioSynthesizer::terminate() {
    if (!this->close()) {
        std::cerr << "Failed to close audio synthesizer." << std::endl;
        return false;
    }
    return true;
}


void AudioSynthesizer::record() {
    if (!m_audio_recorder->is_initialized()) {
        std::cerr << "Audio recorder is not initialized." << std::endl;
        return;
    }
    m_audio_playback->stop();
    m_audio_recorder->record(0);
}

void AudioSynthesizer::stop_recording() {
    if (!m_audio_recorder->is_initialized()) {
        std::cerr << "Audio recorder is not initialized." << std::endl;
        return;
    }
    m_audio_recorder->stop();
}

void AudioSynthesizer::play_recording() {
    if (!m_audio_recorder->is_initialized()) {
        std::cerr << "Audio recorder is not initialized." << std::endl;
        return;
    }
    m_audio_recorder->stop();
    m_audio_playback->load_tape(m_audio_recorder->get_tape());
    m_audio_playback->play(0);
}

