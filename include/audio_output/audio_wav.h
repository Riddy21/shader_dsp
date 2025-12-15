#pragma once
#ifndef AUDIO_WAV_H
#define AUDIO_WAV_H

#include <cstdint>
#include <string>
#include <vector>

struct WAVHeader {
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t overall_size = 0;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt_chunk_marker[4] = {'f', 'm', 't', ' '};
    uint32_t length_of_fmt;
    uint16_t format_type;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data_chunk_header[4] = {'d', 'a', 't', 'a'};
    uint32_t data_size;
};

/**
 * @brief Write float audio data to a WAV file
 * @param output_filepath Path to output WAV file
 * @param audio_data Channel-major format: vector of channels, each channel is a vector of float samples
 * @param sample_rate Sample rate in Hz
 * @param num_channels Number of channels
 * @return true on success, false on failure
 */
bool write_wav_file(const std::string& output_filepath,
                    const std::vector<std::vector<float>>& audio_data,
                    unsigned int sample_rate,
                    unsigned int num_channels);

#endif