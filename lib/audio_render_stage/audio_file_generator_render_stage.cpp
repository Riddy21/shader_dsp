#include <iostream>
#include <fstream>
#include <cstring>

#include "audio_output/audio_wav.h"
#include "audio_parameter/audio_texture2d_parameter.h"
#include "audio_core/audio_render_stage.h"
#include "audio_render_stage/audio_file_generator_render_stage.h"
#include "audio_render_stage_plugins/audio_render_stage_history.h"
#include "audio_core/audio_tape.h"

AudioSingleShaderFileGeneratorRenderStage::AudioSingleShaderFileGeneratorRenderStage(const unsigned int frames_per_buffer,
                                                             const unsigned int sample_rate,
                                                             const unsigned int num_channels,
                                                             const std::string & audio_filepath)
    : AudioSingleShaderGeneratorRenderStage(frames_per_buffer, sample_rate, num_channels, "build/shaders/file_generator_render_stage.glsl",
                                            std::vector<std::string>{
                                                "build/shaders/global_settings.glsl",
                                                "build/shaders/frag_shader_settings.glsl",
                                                "build/shaders/generator_render_stage_settings.glsl",
                                                "build/shaders/adsr_envelope.glsl",
                                                "build/shaders/tape_history_settings.glsl"}),
      AudioFileGeneratorRenderStageBase(audio_filepath) {

    // Load audio file into AudioTape
    m_tape = AudioTape::load_from_wav_file(audio_filepath, frames_per_buffer, sample_rate);
    if (!m_tape) {
        std::cerr << "Failed to load audio file into tape: " << audio_filepath << std::endl;
        exit(1);
    }

    // Create tape history with a window size large enough to cover the entire tape
    // Calculate window size from tape duration (add some margin for safety)
    float tape_duration_seconds = static_cast<float>(m_tape->size()) / static_cast<float>(sample_rate);
    float WINDOW_SIZE_SECONDS = tape_duration_seconds + 1.0f; // Add 1 second margin
    m_history2 = std::make_unique<AudioRenderStageHistory2>(frames_per_buffer, sample_rate, num_channels, WINDOW_SIZE_SECONDS);
    m_history2->create_parameters(m_active_texture_count++);
    m_history2->set_tape(m_tape);
    
    // Set up tape for playback
    m_history2->set_tape_speed(1.0f); // Normal speed
    m_history2->set_tape_position(0u);
    m_history2->start_tape();

    // Add all history parameters
    auto params = m_history2->get_parameters();
    for (auto* param : params) {
        this->add_parameter(param);
    }

    // Initialize window before first render
    m_history2->update_window();
}

AudioSingleShaderFileGeneratorRenderStage::AudioSingleShaderFileGeneratorRenderStage(const std::string & stage_name,
                                                             const unsigned int frames_per_buffer,
                                                             const unsigned int sample_rate,
                                                             const unsigned int num_channels,
                                                             const std::string & audio_filepath)
    : AudioSingleShaderGeneratorRenderStage(stage_name, frames_per_buffer, sample_rate, num_channels, "build/shaders/file_generator_render_stage.glsl",
                                            std::vector<std::string>{
                                                "build/shaders/global_settings.glsl",
                                                "build/shaders/frag_shader_settings.glsl",
                                                "build/shaders/generator_render_stage_settings.glsl",
                                                "build/shaders/adsr_envelope.glsl",
                                                "build/shaders/tape_history_settings.glsl"}),
      AudioFileGeneratorRenderStageBase(audio_filepath) {

    // Load audio file into AudioTape
    m_tape = AudioTape::load_from_wav_file(audio_filepath, frames_per_buffer, sample_rate);
    if (!m_tape) {
        std::cerr << "Failed to load audio file into tape: " << audio_filepath << std::endl;
        exit(1);
    }

    // Create tape history with a window size large enough to cover the entire tape
    // Calculate window size from tape duration (add some margin for safety)
    float tape_duration_seconds = static_cast<float>(m_tape->size()) / static_cast<float>(sample_rate);
    float WINDOW_SIZE_SECONDS = tape_duration_seconds + 1.0f; // Add 1 second margin
    m_history2 = std::make_unique<AudioRenderStageHistory2>(frames_per_buffer, sample_rate, num_channels, WINDOW_SIZE_SECONDS);
    m_history2->create_parameters(m_active_texture_count++);
    m_history2->set_tape(m_tape);
    
    // Set up tape for playback
    m_history2->set_tape_speed(1.0f); // Normal speed
    m_history2->set_tape_position(0u);
    m_history2->start_tape();

    // Add all history parameters
    auto params = m_history2->get_parameters();
    for (auto* param : params) {
        this->add_parameter(param);
    }

    // Initialize window before first render
    m_history2->update_window();
}

AudioFileGeneratorRenderStage::AudioFileGeneratorRenderStage(const unsigned int frames_per_buffer,
                                                             const unsigned int sample_rate,
                                                             const unsigned int num_channels,
                                                             const std::string & audio_filepath)
    : AudioGeneratorRenderStage(frames_per_buffer, sample_rate, num_channels, "build/shaders/multinote_file_generator_render_stage.glsl",
                                std::vector<std::string>{
                                    "build/shaders/global_settings.glsl",
                                    "build/shaders/frag_shader_settings.glsl",
                                    "build/shaders/multinote_generator_render_stage_settings.glsl",
                                    "build/shaders/adsr_envelope.glsl",
                                    "build/shaders/tape_history_settings.glsl"}),
      AudioFileGeneratorRenderStageBase(audio_filepath) {

    // Load audio file into AudioTape
    m_tape = AudioTape::load_from_wav_file(audio_filepath, frames_per_buffer, sample_rate);
    if (!m_tape) {
        std::cerr << "Failed to load audio file into tape: " << audio_filepath << std::endl;
        exit(1);
    }

    // Create tape history with a window size large enough to cover the entire tape
    // Calculate window size from tape duration (add some margin for safety)
    float tape_duration_seconds = static_cast<float>(m_tape->size()) / static_cast<float>(sample_rate);
    float WINDOW_SIZE_SECONDS = tape_duration_seconds + 1.0f; // Add 1 second margin
    m_history2 = std::make_unique<AudioRenderStageHistory2>(frames_per_buffer, sample_rate, num_channels, WINDOW_SIZE_SECONDS);
    m_history2->create_parameters(m_active_texture_count++);
    m_history2->set_tape(m_tape);
    
    // Set up tape for playback
    m_history2->set_tape_speed(1.0f); // Normal speed
    m_history2->set_tape_position(0u);
    m_history2->start_tape();

    // Add all history parameters
    auto params = m_history2->get_parameters();
    for (auto* param : params) {
        this->add_parameter(param);
    }

    // Initialize window before first render
    m_history2->update_window();
}

AudioFileGeneratorRenderStage::AudioFileGeneratorRenderStage(const std::string & stage_name,
                                                             const unsigned int frames_per_buffer,
                                                             const unsigned int sample_rate,
                                                             const unsigned int num_channels,
                                                             const std::string & audio_filepath)
    : AudioGeneratorRenderStage(stage_name, frames_per_buffer, sample_rate, num_channels, "build/shaders/multinote_file_generator_render_stage.glsl",
                                std::vector<std::string>{
                                    "build/shaders/global_settings.glsl",
                                    "build/shaders/frag_shader_settings.glsl",
                                    "build/shaders/multinote_generator_render_stage_settings.glsl",
                                    "build/shaders/adsr_envelope.glsl",
                                    "build/shaders/tape_history_settings.glsl"}),
      AudioFileGeneratorRenderStageBase(audio_filepath) {

    // Load audio file into AudioTape
    m_tape = AudioTape::load_from_wav_file(audio_filepath, frames_per_buffer, sample_rate);
    if (!m_tape) {
        std::cerr << "Failed to load audio file into tape: " << audio_filepath << std::endl;
        exit(1);
    }

    // Create tape history with a window size large enough to cover the entire tape
    // Calculate window size from tape duration (add some margin for safety)
    float tape_duration_seconds = static_cast<float>(m_tape->size()) / static_cast<float>(sample_rate);
    float WINDOW_SIZE_SECONDS = tape_duration_seconds + 1.0f; // Add 1 second margin
    m_history2 = std::make_unique<AudioRenderStageHistory2>(frames_per_buffer, sample_rate, num_channels, WINDOW_SIZE_SECONDS);
    m_history2->create_parameters(m_active_texture_count++);
    m_history2->set_tape(m_tape);
    
    // Set up tape for playback
    m_history2->set_tape_speed(1.0f); // Normal speed
    m_history2->set_tape_position(0u);
    m_history2->start_tape();

    // Add all history parameters
    auto params = m_history2->get_parameters();
    for (auto* param : params) {
        this->add_parameter(param);
    }

    // Initialize window before first render
    m_history2->update_window();
}

void AudioSingleShaderFileGeneratorRenderStage::render(const unsigned int time) {
    AudioSingleShaderGeneratorRenderStage::render(time);
}

void AudioFileGeneratorRenderStage::render(const unsigned int time) {
    // Call base class render (AudioRenderStage) since AudioGeneratorRenderStage::render() is private
    AudioRenderStage::render(time);
}
