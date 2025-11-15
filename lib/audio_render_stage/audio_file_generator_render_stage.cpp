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
    m_tape = AudioTape::load_from_wav_file(audio_filepath, frames_per_buffer, sample_rate, num_channels);
    if (!m_tape) {
        std::cerr << "Failed to load audio file into tape: " << audio_filepath << std::endl;
        exit(1);
    }

    // Create tape history with a window size large enough to cover the entire tape
    // Calculate window size from tape duration (add some margin for safety)
    float tape_duration_seconds = static_cast<float>(m_tape->size()) / static_cast<float>(sample_rate);
    float WINDOW_SIZE_SECONDS = tape_duration_seconds + 1.0f; // Add 1 second margin
    m_history2 = std::make_unique<AudioRenderStageHistory2>(frames_per_buffer, sample_rate, num_channels, WINDOW_SIZE_SECONDS);
    m_history2->create_parameters(m_active_texture_count);
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
    m_tape = AudioTape::load_from_wav_file(audio_filepath, frames_per_buffer, sample_rate, num_channels);
    if (!m_tape) {
        std::cerr << "Failed to load audio file into tape: " << audio_filepath << std::endl;
        exit(1);
    }

    // Create tape history with a window size large enough to cover the entire tape
    // Calculate window size from tape duration (add some margin for safety)
    float tape_duration_seconds = static_cast<float>(m_tape->size()) / static_cast<float>(sample_rate);
    float WINDOW_SIZE_SECONDS = tape_duration_seconds + 1.0f; // Add 1 second margin
    m_history2 = std::make_unique<AudioRenderStageHistory2>(frames_per_buffer, sample_rate, num_channels, WINDOW_SIZE_SECONDS);
    m_history2->create_parameters(m_active_texture_count);
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
    m_tape = AudioTape::load_from_wav_file(audio_filepath, frames_per_buffer, sample_rate, num_channels);
    if (!m_tape) {
        std::cerr << "Failed to load audio file into tape: " << audio_filepath << std::endl;
        exit(1);
    }

    // Create tape history with a window size large enough to cover the entire tape
    // Calculate window size from tape duration (add some margin for safety)
    float tape_duration_seconds = static_cast<float>(m_tape->size()) / static_cast<float>(sample_rate);
    float WINDOW_SIZE_SECONDS = tape_duration_seconds + 1.0f; // Add 1 second margin
    m_history2 = std::make_unique<AudioRenderStageHistory2>(frames_per_buffer, sample_rate, num_channels, WINDOW_SIZE_SECONDS);
    m_history2->create_parameters(m_active_texture_count);
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
    m_tape = AudioTape::load_from_wav_file(audio_filepath, frames_per_buffer, sample_rate, num_channels);
    if (!m_tape) {
        std::cerr << "Failed to load audio file into tape: " << audio_filepath << std::endl;
        exit(1);
    }

    // Create tape history with a window size large enough to cover the entire tape
    // Calculate window size from tape duration (add some margin for safety)
    float tape_duration_seconds = static_cast<float>(m_tape->size()) / static_cast<float>(sample_rate);
    float WINDOW_SIZE_SECONDS = tape_duration_seconds + 1.0f; // Add 1 second margin
    m_history2 = std::make_unique<AudioRenderStageHistory2>(frames_per_buffer, sample_rate, num_channels, WINDOW_SIZE_SECONDS);
    m_history2->create_parameters(m_active_texture_count);
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

const std::vector<float> AudioFileGeneratorRenderStageBase::load_audio_data_from_file(const std::string & audio_filepath) {
    // Open the audio file
    std::ifstream file(audio_filepath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open audio file: " << audio_filepath << std::endl;
        exit(1);
    }

    // Read the audio file header
    WAVHeader header;
    file.read((char *) &header, sizeof(WAVHeader));

    if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0) {
        std::cerr << "Invalid audio file format: " << audio_filepath << std::endl;
        exit(1);
    }

    if (header.format_type != 1) {
        std::cerr << "Invalid audio file format type: " << audio_filepath << std::endl;
        exit(1);
    }

    // print info 
    std::cout << "Audio file: " << audio_filepath << std::endl;
    std::cout << "Channels: " << header.channels << std::endl;
    std::cout << "Sample rate: " << header.sample_rate << std::endl;
    std::cout << "Bits per sample: " << header.bits_per_sample << std::endl;
    std::cout << "Data size: " << header.data_size << std::endl;

    // Read the audio data
    std::vector<int16_t> data(header.data_size/sizeof(int16_t));
    file.read((char *) data.data(), header.data_size);

    if (!file) {
        std::cerr << "Failed to read audio data from file: " << audio_filepath << std::endl;
        exit(1);
    }

    // Convert the audio data to float
    std::vector<std::vector<float>> audio_data(header.channels, std::vector<float>(data.size()/header.channels));
    for (unsigned int i = 0; i < data.size(); i++) {
        audio_data[i % header.channels][i / header.channels] = data[i] / 32768.0f; // have to shift the wave from -1.0 to 1.0
    }

    // Determine width and height based on MAX_TEXTURE_SIZE round to nearest multiple of MAX_TEXTURE_SIZE
    const unsigned int width = (unsigned)MAX_TEXTURE_SIZE;
    const unsigned int num_lines_of_data = audio_data[0].size() / width + 1;
    const unsigned int height = num_lines_of_data * header.channels * 2; // 2 for the 0s in between the audio data

    std::vector<float> full_audio_data;

    // Add audio data to full_audio_data vector

    // Store audio in the form of 
    // c1 c1 c1 c1 c1 c1 c1 c1 c1 c1
    // 0  0  0  0  0  0  0  0  0  0
    // c2 c2 c2 c2 c2 c2 c2 c2 c2 c2
    // 0  0  0  0  0  0  0  0  0  0
    // c1 c1 c1 c1 c1 c1 c1 c1 c1 c1
    // ....

    for (unsigned int i = 0; i < num_lines_of_data; i++) {
        for (unsigned int j = 0; j < header.channels; j++) {
            std::vector<float> data;
            auto start = audio_data[j].begin() + i * width;
            auto end = audio_data[j].begin() + (i + 1) * width;
            // add 0s to the end of the audio data
            if (end > audio_data[j].end()) {
                full_audio_data.insert(full_audio_data.end(), start, audio_data[j].end());
                full_audio_data.insert(full_audio_data.end(), width - (audio_data[j].end() - start), 0.0f);
            } else {
                full_audio_data.insert(full_audio_data.end(), start, end);
            }
            // Push back buffer of 0s
            full_audio_data.insert(full_audio_data.end(), width, 0.0f);
        }
    }

    return full_audio_data;
}

void AudioSingleShaderFileGeneratorRenderStage::render(const unsigned int time) {
    if (m_history2) {
        // For file generator, the shader calculates tape position independently
        // We need to ensure the window covers the entire tape for now
        // TODO: Optimize to only update window when needed based on shader-calculated positions
        m_history2->update_window();
    }
    AudioSingleShaderGeneratorRenderStage::render(time);
}

void AudioFileGeneratorRenderStage::render(const unsigned int time) {
    if (m_history2) {
        // For file generator, the shader calculates tape position independently
        // We need to ensure the window covers the entire tape for now
        // TODO: Optimize to only update window when needed based on shader-calculated positions
        m_history2->update_window();
    }
    // Call base class render (AudioRenderStage) since AudioGeneratorRenderStage::render() is private
    AudioRenderStage::render(time);
}
