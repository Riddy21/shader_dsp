#include "audio_synthesizer/audio_synthesizer.h"

#include "audio_core/audio_renderer.h"
#include "audio_core/audio_render_graph.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_output/audio_player_output.h"


// Initialize the static instance pointer
AudioSynthesizer* AudioSynthesizer::instance = nullptr;

AudioSynthesizer::AudioSynthesizer() :
        m_audio_renderer(AudioRenderer::get_instance()) {
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
    if (!m_synthesizer_thread.add_task([this]() { return m_audio_renderer.initialize(m_buffer_size, m_sample_rate, m_num_channels); }).get()) {
        std::cerr << "Failed to initialize audio renderer." << std::endl;
        return false;
    }

    m_audio_renderer.set_lead_output(0);

    // FIXME: Delete this after testing
    m_audio_generator = new AudioGeneratorRenderStage(m_buffer_size, m_sample_rate, m_num_channels, "build/shaders/multinote_sine_generator_render_stage.glsl");
    m_synthesizer_thread.add_task([this]() {
        m_render_graph->insert_render_stage_infront(m_render_stages["final"]->gid, m_audio_generator);
    }).get();

    return true;
}

// FIXME: Delete this after testing / or modify to use engine object
void AudioSynthesizer::play_note(const float tone, const float volume) {
    if (!m_audio_generator->is_initialized()) {
        std::cerr << "Audio generator is not initialized." << std::endl;
        return;
    }
    m_audio_generator->play_note(tone, volume);
}

// FIXME: Delete this after testing / or modify to use engine object
void AudioSynthesizer::stop_note(const float tone) {
    if (!m_audio_generator->is_initialized()) {
        std::cerr << "Audio generator is not initialized." << std::endl;
        return;
    }
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

    m_synthesizer_thread.add_task([this]() {
        m_audio_renderer.start_main_loop();
    });

    return true;
}

bool AudioSynthesizer::pause() {
    if (!m_audio_renderer.is_initialized()) {
        std::cerr << "Audio renderer is not initialized." << std::endl;
        return false;
    }
    m_audio_renderer.pause_main_loop();
    
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
    if (!m_audio_renderer.is_initialized()) {
        std::cerr << "Audio renderer is not initialized." << std::endl;
        return false;
    }
    m_audio_renderer.increment_main_loop();
    return true;
}

bool AudioSynthesizer::resume() {
    if (!m_audio_renderer.is_initialized()) {
        std::cerr << "Audio renderer is not initialized." << std::endl;
        return false;
    }

    // Start the audio outputs
    for (auto& output : m_render_outputs) {
        if (!output->start()) {
            std::cerr << "Failed to start audio output." << std::endl;
            return false;
        }
    }

    m_audio_renderer.unpause_main_loop();

    return true;
}

bool AudioSynthesizer::close() {
    if (!m_audio_renderer.is_initialized()) {
        std::cerr << "Audio renderer is not initialized." << std::endl;
        return false;
    }

    // Stop the main loop
    m_audio_renderer.terminate();

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
    m_synthesizer_thread.stop();

    if (!this->close()) {
        std::cerr << "Failed to close audio synthesizer." << std::endl;
        return false;
    }
    if (!m_audio_renderer.is_initialized()) {
        std::cerr << "Audio renderer is not initialized." << std::endl;
        return false;
    }
    if (!m_audio_renderer.terminate()) {
        std::cerr << "Failed to terminate audio renderer." << std::endl;
        return false;
    }
    return true;
}
