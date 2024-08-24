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
      audio_filepath(audio_filepath) {
        // Load the audio data filepath into the full_audio_data vector
        full_audio_data = load_audio_data_from_file(audio_filepath);
}

void AudioGeneratorRenderStage::update(const unsigned int buffer_index) {
    // Copy the audio data from the full_audio_data vector in the ith buffer index
    const unsigned int start_index = (buffer_index * frames_per_buffer * num_channels) % full_audio_data.size();

    for (unsigned int i = 0; i < frames_per_buffer * num_channels; i++) {
        audio_buffer[i] = full_audio_data[start_index + i];
    }
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
        // Normalize the audio data to the range [0.0, 1.0]
        audio_data[i] = data[i] / (2.0f * 32768.0f) + 0.5f; // have to shift the wave from 0.0 to 1.0
    }
    return audio_data;


}