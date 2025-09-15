#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <cstring>
#include "audio_output/audio_wav.h"

/**
 * @brief Generate a sine wave buffer
 * @param frequency Frequency in Hz
 * @param amplitude Amplitude (0.0 to 1.0)
 * @param sample_rate Sample rate in Hz
 * @param frames_per_buffer Number of frames per buffer
 * @param channels Number of channels
 * @param phase Initial phase offset
 * @return Vector of float samples
 */
inline std::vector<float> generate_sine_wave(float frequency, float amplitude, 
                                     unsigned sample_rate, unsigned frames_per_buffer, 
                                     unsigned channels, float phase = 0.0f) {
    std::vector<float> buffer(frames_per_buffer * channels);
    
    for (unsigned i = 0; i < frames_per_buffer; ++i) {
        float sample = amplitude * std::sin(2.0f * M_PI * frequency * (i + phase) / sample_rate);
        for (unsigned ch = 0; ch < channels; ++ch) {
            buffer[i * channels + ch] = sample;
        }
    }
    
    return buffer;
}

/**
 * @brief Generate a constant value buffer
 * @param value Constant value to fill buffer with
 * @param frames_per_buffer Number of frames per buffer
 * @param channels Number of channels
 * @return Vector of float samples
 */
inline std::vector<float> generate_constant_buffer(float value, unsigned frames_per_buffer, unsigned channels) {
    std::vector<float> buffer(frames_per_buffer * channels, value);
    return buffer;
}

/**
 * @brief Generate a silence buffer (all zeros)
 * @param frames_per_buffer Number of frames per buffer
 * @param channels Number of channels
 * @return Vector of float samples (all zeros)
 */
inline std::vector<float> generate_silence_buffer(unsigned frames_per_buffer, unsigned channels) {
    return std::vector<float>(frames_per_buffer * channels, 0.0f);
}

/**
 * @brief Generate a noise buffer (random values)
 * @param frames_per_buffer Number of frames per buffer
 * @param channels Number of channels
 * @param amplitude Amplitude of noise (0.0 to 1.0)
 * @return Vector of float samples with random noise
 */
inline std::vector<float> generate_noise_buffer(unsigned frames_per_buffer, unsigned channels, float amplitude = 0.1f) {
    std::vector<float> buffer(frames_per_buffer * channels);
    
    // Simple random number generation (not cryptographically secure, but sufficient for tests)
    static unsigned int seed = 12345;
    
    for (unsigned i = 0; i < frames_per_buffer * channels; ++i) {
        // Generate random number between -1 and 1
        seed = seed * 1103515245 + 12345;
        float random_value = static_cast<float>(static_cast<int>(seed >> 16) & 0x7FFF) / 16384.0f - 1.0f;
        buffer[i] = random_value * amplitude;
    }
    
    return buffer;
}

/**
 * @brief Calculate RMS (Root Mean Square) of float audio data
 * @param audio_data Vector of float audio samples
 * @return RMS value
 */
inline float calculate_rms(const std::vector<float>& audio_data) {
    if (audio_data.empty()) return 0.0f;
    
    float sum_squares = 0.0f;
    for (float sample : audio_data) {
        sum_squares += sample * sample;
    }
    
    return std::sqrt(sum_squares / audio_data.size());
}

/**
 * @brief Calculate RMS (Root Mean Square) of int16_t audio data
 * @param audio_data Vector of int16_t audio samples
 * @return RMS value
 */
inline float calculate_rms_int16(const std::vector<int16_t>& audio_data) {
    if (audio_data.empty()) return 0.0f;
    
    float sum_squares = 0.0f;
    for (int16_t sample : audio_data) {
        float normalized_sample = static_cast<float>(sample) / 32760.0f;
        sum_squares += normalized_sample * normalized_sample;
    }
    
    return std::sqrt(sum_squares / audio_data.size());
}

/**
 * @brief Calculate peak amplitude of float audio data
 * @param audio_data Vector of float audio samples
 * @return Peak amplitude
 */
inline float calculate_peak(const std::vector<float>& audio_data) {
    if (audio_data.empty()) return 0.0f;
    
    float max_amplitude = 0.0f;
    for (float sample : audio_data) {
        max_amplitude = std::max(max_amplitude, std::abs(sample));
    }
    
    return max_amplitude;
}

/**
 * @brief Convert float sample to int16_t
 * @param sample Float sample (-1.0 to 1.0)
 * @return int16_t sample
 */
inline int16_t float_to_int16(float sample) {
    return static_cast<int16_t>(sample * 32760.0f);
}

/**
 * @brief Detect if audio contains the expected frequency (float samples)
 * @param audio_data Vector of float audio samples
 * @param expected_freq Expected frequency in Hz
 * @param sample_rate Sample rate in Hz
 * @param tolerance Frequency tolerance (default 0.2 = 20%)
 * @return true if frequency is detected within tolerance
 */
inline bool detect_frequency(const std::vector<float>& audio_data, float expected_freq, 
                     unsigned sample_rate, float tolerance = 0.2f) {
    if (audio_data.empty()) return false;
    
    // For small buffers, use a more lenient approach
    // Check if we have at least one complete cycle
    float period_samples = sample_rate / expected_freq;
    int min_samples_for_cycle = static_cast<int>(period_samples * 0.5f); // At least half a cycle
    
    if (audio_data.size() < min_samples_for_cycle) {
        // For very small buffers, just verify it's not silence
        float rms = calculate_rms(audio_data);
        return rms > 0.001f; // Has some audio content
    }
    
    // Simple zero-crossing detection for frequency estimation
    int zero_crossings = 0;
    for (size_t i = 1; i < audio_data.size(); ++i) {
        if ((audio_data[i-1] < 0 && audio_data[i] >= 0) || 
            (audio_data[i-1] > 0 && audio_data[i] <= 0)) {
            zero_crossings++;
        }
    }
    
    // Calculate detected frequency
    float detected_freq = (zero_crossings * sample_rate) / (2.0f * audio_data.size());
    
    // Check if detected frequency is within tolerance
    return std::abs(detected_freq - expected_freq) <= (expected_freq * tolerance);
}

/**
 * @brief Detect if audio contains the expected frequency (int16_t samples)
 * @param audio_data Vector of int16_t audio samples
 * @param expected_freq Expected frequency in Hz
 * @param sample_rate Sample rate in Hz
 * @param channels Number of channels
 * @param tolerance Frequency tolerance (default 0.2 = 20%)
 * @return true if frequency is detected within tolerance
 */
inline bool detect_frequency_int16(const std::vector<int16_t>& audio_data, float expected_freq, 
                           unsigned sample_rate, unsigned channels, float tolerance = 0.2f) {
    if (audio_data.empty()) return false;

    // Only analyze the first channel (e.g., left)
    std::vector<float> float_data;
    for (size_t i = 0; i < audio_data.size(); i += channels) {
        float_data.push_back(static_cast<float>(audio_data[i]) / 32760.0f);
    }

    float period_samples = sample_rate / expected_freq;
    int min_samples_for_cycle = static_cast<int>(period_samples * 0.5f);

    if (float_data.size() < min_samples_for_cycle) {
        float rms = 0.0f;
        for (float sample : float_data) rms += sample * sample;
        rms = std::sqrt(rms / float_data.size());
        return rms > 0.001f;
    }

    int zero_crossings = 0;
    for (size_t i = 1; i < float_data.size(); ++i) {
        if ((float_data[i-1] < 0 && float_data[i] >= 0) || 
            (float_data[i-1] > 0 && float_data[i] <= 0)) {
            zero_crossings++;
        }
    }

    float detected_freq = (zero_crossings * sample_rate) / (2.0f * float_data.size());
    return std::abs(detected_freq - expected_freq) <= (expected_freq * tolerance);
}

/**
 * @brief Detect if audio contains the expected frequency on a specific channel (int16_t samples)
 * @param audio_data Vector of int16_t audio samples
 * @param expected_freq Expected frequency in Hz
 * @param sample_rate Sample rate in Hz
 * @param channels Number of channels
 * @param channel_index Channel to analyze
 * @param tolerance Frequency tolerance (default 0.2 = 20%)
 * @return true if frequency is detected within tolerance
 */
inline bool detect_frequency_int16_channel(const std::vector<int16_t>& audio_data, float expected_freq, 
                                   unsigned sample_rate, unsigned channels, unsigned channel_index, 
                                   float tolerance = 0.2f) {
    if (audio_data.empty() || channel_index >= channels) return false;

    // Analyze the specified channel
    std::vector<float> float_data;
    for (size_t i = channel_index; i < audio_data.size(); i += channels) {
        float_data.push_back(static_cast<float>(audio_data[i]) / 32760.0f);
    }

    float period_samples = sample_rate / expected_freq;
    int min_samples_for_cycle = static_cast<int>(period_samples * 0.5f);

    if (float_data.size() < min_samples_for_cycle) {
        float rms = 0.0f;
        for (float sample : float_data) rms += sample * sample;
        rms = std::sqrt(rms / float_data.size());
        return rms > 0.001f;
    }

    int zero_crossings = 0;
    for (size_t i = 1; i < float_data.size(); ++i) {
        if ((float_data[i-1] < 0 && float_data[i] >= 0) || 
            (float_data[i-1] > 0 && float_data[i] <= 0)) {
            zero_crossings++;
        }
    }

    float detected_freq = (zero_crossings * sample_rate) / (2.0f * float_data.size());
    return std::abs(detected_freq - expected_freq) <= (expected_freq * tolerance);
}

/**
 * @brief Validate WAV file header
 * @param filename Path to WAV file
 * @param expected_channels Expected number of channels
 * @param expected_sample_rate Expected sample rate
 * @param expected_bits_per_sample Expected bits per sample
 * @return true if header is valid
 */
inline bool validate_wav_header(const std::string& filename, unsigned expected_channels, 
                        unsigned expected_sample_rate, unsigned expected_bits_per_sample) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    WAVHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));
    file.close();
    
    // Check RIFF and WAVE identifiers
    if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0) {
        return false;
    }
    
    // Check format chunk marker
    if (strncmp(header.fmt_chunk_marker, "fmt ", 4) != 0) {
        return false;
    }
    
    // Check data chunk header
    if (strncmp(header.data_chunk_header, "data", 4) != 0) {
        return false;
    }
    
    // Check format type (PCM = 1)
    if (header.format_type != 1) {
        return false;
    }
    
    // Check expected values
    if (header.channels != expected_channels) {
        return false;
    }
    
    if (header.sample_rate != expected_sample_rate) {
        return false;
    }
    
    if (header.bits_per_sample != expected_bits_per_sample) {
        return false;
    }
    
    // Check calculated values
    uint32_t expected_byte_rate = expected_sample_rate * expected_channels * expected_bits_per_sample / 8;
    if (header.byte_rate != expected_byte_rate) {
        return false;
    }
    
    uint16_t expected_block_align = expected_channels * expected_bits_per_sample / 8;
    if (header.block_align != expected_block_align) {
        return false;
    }
    
    return true;
}

/**
 * @brief Read audio data from WAV file
 * @param filename Path to WAV file
 * @return Vector of int16_t audio samples
 */
inline std::vector<int16_t> read_wav_audio_data(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }
    
    WAVHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));
    
    std::vector<int16_t> audio_data;
    int16_t sample;
    while (file.read(reinterpret_cast<char*>(&sample), sizeof(int16_t))) {
        audio_data.push_back(sample);
    }
    
    file.close();
    return audio_data;
}

/**
 * @brief Clean up test file if it exists
 * @param filename Path to file to remove
 */
inline void cleanup_test_file(const std::string& filename) {
    std::filesystem::remove(filename);
} 