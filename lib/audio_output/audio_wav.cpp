#include "audio_output/audio_wav.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <vector>

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
                    unsigned int num_channels) {
    // Check if audio data is valid
    if (audio_data.empty() || audio_data[0].empty()) {
        std::cerr << "Cannot write empty audio data to WAV file" << std::endl;
        return false;
    }
    
    // Verify all channels have the same size
    const unsigned int samples_per_channel = static_cast<unsigned int>(audio_data[0].size());
    for (unsigned int ch = 1; ch < audio_data.size(); ++ch) {
        if (audio_data[ch].size() != samples_per_channel) {
            std::cerr << "All channels must have the same number of samples" << std::endl;
            return false;
        }
    }
    
    if (audio_data.size() != num_channels) {
        std::cerr << "Number of channels in audio_data (" << audio_data.size() 
                  << ") does not match num_channels (" << num_channels << ")" << std::endl;
        return false;
    }
    
    // Open output file
    std::ofstream file(output_filepath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open output file: " << output_filepath << std::endl;
        return false;
    }
    
    const unsigned int total_samples = samples_per_channel * num_channels;
    const unsigned int data_size_bytes = total_samples * sizeof(int16_t);
    
    // Create WAV header
    WAVHeader header;
    strncpy(header.riff, "RIFF", 4);
    header.overall_size = 36 + data_size_bytes; // 36 = header size - 8 (RIFF + overall_size)
    strncpy(header.wave, "WAVE", 4);
    strncpy(header.fmt_chunk_marker, "fmt ", 4);
    header.length_of_fmt = 16;
    header.format_type = 1; // PCM
    header.channels = static_cast<uint16_t>(num_channels);
    header.sample_rate = sample_rate;
    header.bits_per_sample = 16;
    header.byte_rate = header.sample_rate * header.channels * header.bits_per_sample / 8;
    header.block_align = header.channels * header.bits_per_sample / 8;
    strncpy(header.data_chunk_header, "data", 4);
    header.data_size = data_size_bytes;
    
    // Write header (will update overall_size later)
    file.write((char *) &header, sizeof(WAVHeader));
    
    if (!file) {
        std::cerr << "Failed to write WAV header" << std::endl;
        return false;
    }
    
    // Convert from channel-major to interleaved format and write samples
    // Channel-major: [ch0_sample0, ch0_sample1, ..., ch1_sample0, ch1_sample1, ...]
    // Interleaved: [ch0_sample0, ch1_sample0, ch0_sample1, ch1_sample1, ...]
    for (unsigned int sample_idx = 0; sample_idx < samples_per_channel; ++sample_idx) {
        for (unsigned int ch = 0; ch < num_channels; ++ch) {
            // Get sample value (clamp to [-1.0, 1.0] range)
            float sample_float = 0.0f;
            if (sample_idx < audio_data[ch].size()) {
                sample_float = std::max(-1.0f, std::min(1.0f, audio_data[ch][sample_idx]));
            }
            
            // Convert float to int16_t
            int16_t sample_int16 = static_cast<int16_t>(sample_float * 32767.0f);
            
            // Write sample
            file.write((char *) &sample_int16, sizeof(int16_t));
            
            if (!file) {
                std::cerr << "Failed to write audio data at sample " << sample_idx << ", channel " << ch << std::endl;
                return false;
            }
        }
    }
    
    // Update header with actual file size
    std::streampos file_size = file.tellp();
    header.overall_size = static_cast<uint32_t>(file_size) - 8;
    
    // Rewrite header with correct sizes
    file.seekp(0);
    file.write((char *) &header, sizeof(WAVHeader));
    
    file.close();
    
    if (!file) {
        std::cerr << "Failed to close output file" << std::endl;
        return false;
    }
    
    std::cout << "Successfully wrote WAV file: " << output_filepath << std::endl;
    std::cout << "  Channels: " << num_channels << std::endl;
    std::cout << "  Sample rate: " << sample_rate << std::endl;
    std::cout << "  Samples per channel: " << samples_per_channel << std::endl;
    std::cout << "  Duration: " << (static_cast<float>(samples_per_channel) / static_cast<float>(sample_rate)) << " seconds" << std::endl;
    
    return true;
}

