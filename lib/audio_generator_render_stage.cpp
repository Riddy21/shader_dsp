#include <vector>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>

#include "audio_wav.h"
#include "audio_texture2d_parameter.h"
#include "audio_uniform_parameters.h"
#include "audio_generator_render_stage.h"

AudioGeneratorRenderStage::AudioGeneratorRenderStage(const unsigned int frames_per_buffer,
                                                     const unsigned int sample_rate,
                                                     const unsigned int num_channels,
                                                     const char * audio_filepath)
    : AudioRenderStage(frames_per_buffer, sample_rate, num_channels),
      m_audio_filepath(audio_filepath) {
        // Load the audio data filepath into the full_audio_data vector
        auto full_audio_data = load_audio_data_from_file(audio_filepath);

        // Determine width and height based on MAX_TEXTURE_SIZE round to nearest multiple of MAX_TEXTURE_SIZE
        const unsigned int width = std::max((unsigned)MAX_TEXTURE_SIZE, m_frames_per_buffer * m_num_channels);
        const unsigned int height = full_audio_data.size() / MAX_TEXTURE_SIZE;

        if (height == 0) {
            std::cerr << "Audio data is too small to fill a texture" << std::endl;
            exit(1);
        }

        // Fill the rest with zeros
        full_audio_data.resize(width * height, 0.0f);

        // Buffer the full audio data
        std::vector<float> buffered_full_audio_data(width * height * 3, 0.0f);

        for (unsigned int i = 0; i < height; i++) {
            std::memcpy(&buffered_full_audio_data[i * width * 3], &full_audio_data[i * width], width * sizeof(float));
        }

        // Add new parameter objects to the parameter list
        auto full_audio_texture =
            std::make_unique<AudioTexture2DParameter>("full_audio_data_texture",
                                  AudioParameter::ConnectionType::INITIALIZATION,
                                  width, height*3);
        full_audio_texture->set_value(buffered_full_audio_data.data());

        auto time_parameter =
            std::make_unique<AudioIntParameter>("time",
                                  AudioParameter::ConnectionType::INPUT);
        int value = 0;
        time_parameter->set_value(&value);

        auto tone_parameter =
            std::make_unique<AudioFloatParameter>("tone",
                                  AudioParameter::ConnectionType::INPUT);
        tone_parameter->set_value(new float(1.0f));

        auto play_parameter =
            std::make_unique<AudioIntParameter>("play",
                                  AudioParameter::ConnectionType::INPUT);
        play_parameter->set_value(new int(0));

        auto output_audio_texture =
            std::make_unique<AudioTexture2DParameter>("output_audio_texture",
                      AudioParameter::ConnectionType::OUTPUT,
                      m_frames_per_buffer * m_num_channels, 1);

        if (!this->add_parameter(std::move(full_audio_texture))) {
            std::cerr << "Failed to add beginning_audio_texture" << std::endl;
        }
        if (!this->add_parameter(std::move(output_audio_texture))) {
            std::cerr << "Failed to add output_audio_texture" << std::endl;
        }
        if (!this->add_parameter(std::move(time_parameter))) {
            std::cerr << "Failed to add time_parameter" << std::endl;
        }
        if (!this->add_parameter(std::move(tone_parameter))) {
            std::cerr << "Failed to add tone_parameter" << std::endl;
        }
        if (!this->add_parameter(std::move(play_parameter))) {
            std::cerr << "Failed to add play_parameter" << std::endl;
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
    std::vector<int16_t> data(header.data_size/sizeof(int16_t));
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