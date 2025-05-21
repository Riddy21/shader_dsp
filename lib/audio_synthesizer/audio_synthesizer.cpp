#include <algorithm>
#include "audio_synthesizer/audio_synthesizer.h"
#include "audio_core/audio_renderer.h"
#include "audio_core/audio_render_graph.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_render_stage/audio_tape_render_stage.h"
#include "audio_render_stage/audio_effect_render_stage.h"
#include "audio_render_stage/audio_multitrack_join_render_stage.h"
#include "audio_render_stage/audio_file_generator_render_stage.h"
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
    m_audio_renderer.set_current_context();

    m_buffer_size = buffer_size;
    m_sample_rate = sample_rate;
    m_num_channels = num_channels;

    // Create the render stages
    m_final_render_stage = new AudioFinalRenderStage(m_buffer_size, m_sample_rate, m_num_channels);

    // Create the multitrack join render stage
    m_audio_join = new AudioMultitrackJoinRenderStage(m_buffer_size, m_sample_rate, m_num_channels, 2);

    m_audio_join->connect_render_stage(m_final_render_stage);

    // Initialize the render graph
    m_render_graph = new AudioRenderGraph(m_final_render_stage);

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

    // Initialzie tracks
    AudioTrack * track = new AudioTrack(m_render_graph, m_audio_join, m_buffer_size, m_sample_rate, m_num_channels);
    add_track(track);

    return true;
}

bool AudioSynthesizer::start() {
    m_audio_renderer.set_current_context();
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
    m_audio_renderer.set_current_context();
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
    m_audio_renderer.set_current_context();
    m_audio_renderer.increment();

    return true;
}

bool AudioSynthesizer::resume() {
    m_audio_renderer.set_current_context();
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
    m_audio_renderer.set_current_context();
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
    m_audio_renderer.set_current_context();
    if (!this->close()) {
        std::cerr << "Failed to close audio synthesizer." << std::endl;
        return false;
    }
    return true;
}

bool AudioSynthesizer::add_track(AudioTrack * track) {
    if (track == nullptr) {
        std::cerr << "Error: Track is null." << std::endl;
        return false;
    }
    m_tracks.push_back(std::unique_ptr<AudioTrack>(track));
    return true;
}

bool AudioSynthesizer::remove_track(AudioTrack * track) {
    if (track == nullptr) {
        std::cerr << "Error: Track is null." << std::endl;
        return false;
    }
    auto it = std::find_if(m_tracks.begin(), m_tracks.end(),
                           [track](const std::unique_ptr<AudioTrack>& t) { return t.get() == track; });
    if (it != m_tracks.end()) {
        m_tracks.erase(it);
        return true;
    }
    return false;
}