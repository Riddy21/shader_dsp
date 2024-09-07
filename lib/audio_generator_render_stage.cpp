#include <vector>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>

#include "audio_wav.h"
#include "audio_generator_render_stage.h"

AudioGeneratorRenderStage::AudioGeneratorRenderStage(const unsigned int frames_per_buffer,
                                                     const unsigned int sample_rate,
                                                     const unsigned int num_channels,
                                                     const char * audio_filepath)
    : AudioRenderStage(frames_per_buffer, sample_rate, num_channels),
      m_audio_filepath(audio_filepath) {
        // Load the audio data filepath into the full_audio_data vector
        m_full_audio_data = load_audio_data_from_file(audio_filepath);

        // Create parameters for the audio generator ports in shader
        AudioRenderStageParameter input_audio_texture_parameter = 
                AudioRenderStageParameter("input_audio_texture",
                                          AudioRenderStageParameter::Type::STREAM_INPUT,
                                          m_frames_per_buffer * m_num_channels,
                                          1,
                                          &m_audio_buffer);

        AudioRenderStageParameter stream_audio_texture_parameter = 
                AudioRenderStageParameter("stream_audio_texture",
                                          AudioRenderStageParameter::Type::STREAM_INPUT,
                                          m_frames_per_buffer * m_num_channels,
                                          1,
                                          nullptr,
                                          "output_audio_texture");

        AudioRenderStageParameter output_audio_texture_parameter =
                AudioRenderStageParameter("output_audio_texture",
                                          AudioRenderStageParameter::Type::STREAM_OUTPUT,
                                          m_frames_per_buffer * m_num_channels,
                                          1,
                                          nullptr,
                                          "stream_audio_texture");

        if (!this->add_parameter(input_audio_texture_parameter)) {
            std::cerr << "Failed to add input_audio_texture_parameter" << std::endl;
        }
        if (!this->add_parameter(stream_audio_texture_parameter)) {
            std::cerr << "Failed to add stream_audio_texture_parameter" << std::endl;
        }
        if (!this->add_parameter(output_audio_texture_parameter)) {
            std::cerr << "Failed to add output_audio_texture_parameter" << std::endl;
        }
}

void AudioGeneratorRenderStage::update() {
    // Copy the audio data from the full_audio_data vector in the ith buffer index
    const unsigned int start_index = (m_play_index * m_frames_per_buffer * m_num_channels) % m_full_audio_data.size();

    // Make sure that the audio buffer is not out of bounds
    if (m_play_index * m_frames_per_buffer * m_num_channels >= m_full_audio_data.size()) {
        stop();
    }

    if (m_is_playing) {
        m_audio_buffer = &m_full_audio_data[start_index];
    } else {
        m_audio_buffer = m_empty_audio_data.data();
    }

    m_play_index++;

}

void AudioGeneratorRenderStage::play() {
    m_play_index = 0;
    m_is_playing = true;
}

void AudioGeneratorRenderStage::stop() {
    m_play_index = 0;
    m_is_playing = false;
}

std::vector<float> AudioGeneratorRenderStage::load_audio_data_from_file(const char * audio_filepath) {
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
    std::vector<int16_t> data(header.data_size);
    file.read((char *) data.data(), header.data_size);

    if (!file) {
        std::cerr << "Failed to read audio data from file: " << audio_filepath << std::endl;
        exit(1);
    }

    // Convert the audio data to float
    std::vector<float> audio_data(data.size());
    for (unsigned int i = 0; i < data.size(); i++) {
        audio_data[i] = data[i] / 32768.0f; // have to shift the wave from -1.0 to 1.0
    }
    return audio_data;


}