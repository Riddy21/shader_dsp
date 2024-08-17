#include <vector>
#include <iostream>
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
    FILE *file = fopen(audio_filepath, "rb");
    if (!file) {
        std::cerr << "Failed to open audio file: " << audio_filepath << std::endl;
        exit(1);
    }

    // Read the audio file header
    char header[44];
    if (fread(header, sizeof(char), 44, file) != 44) {
        std::cerr << "Failed to read audio file header" << std::endl;
        fclose(file);
        exit(1);
    }

    // Check if the audio file format is supported
    if (header[0] != 'R' || header[1] != 'I' || header[2] != 'F' || header[3] != 'F' ||
        header[8] != 'W' || header[9] != 'A' || header[10] != 'V' || header[11] != 'E') {
        std::cerr << "Unsupported audio file format" << std::endl;
        fclose(file);
        exit(1);
    }

    // Convert header to useful information and print out
    unsigned int sample_rate = *(unsigned int*)&header[24];
    unsigned int num_channels = *(int16_t*)&header[22];
    unsigned int bits_per_sample = *(int16_t*)&header[34];
    unsigned int num_samples = *(int16_t*)&header[40];
    std::cout << "Audio File: " << audio_filepath << std::endl;
    std::cout << "Sample Rate: " << sample_rate << std::endl;
    std::cout << "Number of Channels: " << num_channels << std::endl;
    std::cout << "Bits per Sample: " << bits_per_sample << std::endl;
    std::cout << "Number of Samples: " << num_samples << std::endl;

    // Read the audio data
    // FIXME: Change this to store the data using multiple channel vectors
    std::vector<float> audio_data;
    while (!feof(file)) {
        int16_t sample;
        if (fread(&sample, sizeof(int16_t), 1, file) != 1) {
            break;
        }
        const float normalization_factor = 32768.0f;
        audio_data.push_back(sample/normalization_factor);
    }

    // Close the audio file
    fclose(file);

    // Store the audio data in the full_audio_data vector
    return audio_data;
}