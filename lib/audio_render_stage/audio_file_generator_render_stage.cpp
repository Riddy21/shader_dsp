#include <iostream>
#include <fstream>
#include <cstring>

#include "audio_output/audio_wav.h"
#include "audio_parameter/audio_texture2d_parameter.h"
#include "audio_render_stage/audio_render_stage.h"
#include "audio_render_stage/audio_file_generator_render_stage.h"

AudioSingleShaderFileGeneratorRenderStage::AudioSingleShaderFileGeneratorRenderStage(const unsigned int frames_per_buffer,
                                                             const unsigned int sample_rate,
                                                             const unsigned int num_channels,
                                                             const std::string & audio_filepath)
    : AudioSingleShaderGeneratorRenderStage(frames_per_buffer, sample_rate, num_channels, "build/shaders/file_generator_render_stage.glsl"),
      m_audio_filepath(audio_filepath) {

    // Load the audio data filepath into the full_audio_data vector
    auto full_audio_data = load_audio_data_from_file(audio_filepath);

    const unsigned int width = (unsigned)MAX_TEXTURE_SIZE;
    const unsigned int height = full_audio_data.size() / width;

    // Add new parameter objects to the parameter list
    auto full_audio_texture =
        new AudioTexture2DParameter("full_audio_data_texture",
                              AudioParameter::ConnectionType::INITIALIZATION,
                              width, height,
                              ++m_active_texture_count,
                              0, GL_LINEAR);

    full_audio_texture->set_value(full_audio_data.data());

    if (!this->add_parameter(full_audio_texture)) {
        std::cerr << "Failed to add beginning_audio_texture" << std::endl;
    }
}

const std::vector<float> AudioSingleShaderFileGeneratorRenderStage::load_audio_data_from_file(const std::string & audio_filepath) {
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