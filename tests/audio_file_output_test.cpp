#include "catch2/catch_all.hpp"
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <fstream>
#include <filesystem>

#include "audio_output/audio_file_output.h"
#include "audio_output/audio_wav.h"

// Helper function to generate a sine wave
std::vector<float> generate_sine_wave(float frequency, float amplitude, 
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

// Helper function to generate a constant value buffer
std::vector<float> generate_constant_buffer(float value, unsigned frames_per_buffer, unsigned channels) {
    std::vector<float> buffer(frames_per_buffer * channels, value);
    return buffer;
}

// Helper function to read WAV file and validate header
bool validate_wav_header(const std::string& filename, unsigned expected_channels, 
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

// Helper function to read audio data from WAV file
std::vector<int16_t> read_wav_audio_data(const std::string& filename) {
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

// Helper function to convert float samples to int16_t (same as in AudioFileOutput)
int16_t float_to_int16(float sample) {
    return static_cast<int16_t>(sample * 32760.0f);
}

// Helper function to calculate RMS of int16_t audio data
float calculate_rms_int16(const std::vector<int16_t>& audio_data) {
    if (audio_data.empty()) return 0.0f;
    
    float sum_squares = 0.0f;
    for (int16_t sample : audio_data) {
        float normalized_sample = static_cast<float>(sample) / 32760.0f;
        sum_squares += normalized_sample * normalized_sample;
    }
    
    return std::sqrt(sum_squares / audio_data.size());
}

// Helper function to detect if audio contains the expected frequency (for int16_t data)
bool detect_frequency_int16(const std::vector<int16_t>& audio_data, float expected_freq, 
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

// Helper function to detect frequency on a specific channel
bool detect_frequency_int16_channel(const std::vector<int16_t>& audio_data, float expected_freq, 
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

TEST_CASE("AudioFileOutput basic functionality") {
    const unsigned frames_per_buffer = 512;
    const unsigned sample_rate = 44100;
    const unsigned channels = 2;
    const std::string test_filename = "build/tests/test_output.wav";
    
    // Clean up any existing test file
    std::filesystem::remove(test_filename);
    
    SECTION("Open and close audio file") {
        AudioFileOutput file_output(frames_per_buffer, sample_rate, channels, test_filename);
        REQUIRE(file_output.open() == true);
        REQUIRE(file_output.close() == true);
        
        // When just opening and closing without starting, no WAV header is written
        // So we just verify the file exists but don't check WAV header
        REQUIRE(std::filesystem::exists(test_filename));
    }
    
    SECTION("Start and stop audio file") {
        AudioFileOutput file_output(frames_per_buffer, sample_rate, channels, test_filename);
        REQUIRE(file_output.open() == true);
        REQUIRE(file_output.start() == true);
        REQUIRE(file_output.stop() == true);
        REQUIRE(file_output.close() == true);
        
        // Verify file was created and has valid WAV header
        REQUIRE(validate_wav_header(test_filename, channels, sample_rate, 16));
    }
    
    SECTION("Check ready state") {
        AudioFileOutput file_output(frames_per_buffer, sample_rate, channels, test_filename);
        REQUIRE(file_output.open() == true);
        REQUIRE(file_output.start() == true);
        
        // The is_ready() function has timing logic, so we need to wait a bit
        // or call it multiple times to get past the timing restriction
        bool ready = false;
        for (int i = 0; i < 10; ++i) {
            if (file_output.is_ready()) {
                ready = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        REQUIRE(ready == true);
        
        REQUIRE(file_output.stop() == true);
        REQUIRE(file_output.close() == true);
    }
    
    SECTION("Write audio data to file") {
        AudioFileOutput file_output(frames_per_buffer, sample_rate, channels, test_filename);
        
        REQUIRE(file_output.open() == true);
        REQUIRE(file_output.start() == true);
        
        // Generate and write a sine wave
        auto buffer = generate_sine_wave(440.0f, 0.3f, sample_rate, frames_per_buffer, channels);
        file_output.push(buffer.data());
        
        REQUIRE(file_output.stop() == true);
        REQUIRE(file_output.close() == true);
        
        // Verify file was created and has valid WAV header
        REQUIRE(validate_wav_header(test_filename, channels, sample_rate, 16));
        
        // Read and verify audio data
        auto audio_data = read_wav_audio_data(test_filename);
        REQUIRE(!audio_data.empty());
        
        // Should have the expected number of samples
        REQUIRE(audio_data.size() == frames_per_buffer * channels);
        
        // Should contain the expected frequency
        // For small amounts of data, just check that it's not silence
        if (audio_data.size() >= sample_rate * 0.1) { // At least 0.1 seconds
            REQUIRE(detect_frequency_int16(audio_data, 440.0f, sample_rate, channels));
        } else {
            float rms = calculate_rms_int16(audio_data);
            REQUIRE(rms > 0.001f); // Not silence
        }
    }
}

TEST_CASE("AudioFileOutput sine wave writing") {
    const unsigned frames_per_buffer = 512;
    const unsigned sample_rate = 44100;
    const unsigned channels = 2;
    const float frequency = 440.0f; // A4 note
    const float amplitude = 0.3f;
    const std::string test_filename = "build/tests/sine_wave_test.wav";
    
    // Clean up any existing test file
    std::filesystem::remove(test_filename);
    
    SECTION("Write a simple sine wave") {
        AudioFileOutput file_output(frames_per_buffer, sample_rate, channels, test_filename);
        
        REQUIRE(file_output.open() == true);
        REQUIRE(file_output.start() == true);
        
        // Generate and write sine wave for 1 second
        const unsigned num_buffers = sample_rate / frames_per_buffer;
        float phase = 0.0f;
        
        for (unsigned i = 0; i < num_buffers; ++i) {
            // Wait until the file is ready for more audio
            // The is_ready() function has timing restrictions, so we need to be patient
            int wait_count = 0;
            while (!file_output.is_ready() && wait_count < 100) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                wait_count++;
            }
            // If still not ready after waiting, just proceed anyway
            // Generate sine wave buffer
            auto buffer = generate_sine_wave(frequency, amplitude, sample_rate, 
                                           frames_per_buffer, channels, phase);
            // Write audio data
            file_output.push(buffer.data());
            // Update phase for next buffer
            phase += frames_per_buffer;
        }
        
        REQUIRE(file_output.stop() == true);
        REQUIRE(file_output.close() == true);
        
        // Verify file was created and has valid WAV header
        REQUIRE(validate_wav_header(test_filename, channels, sample_rate, 16));
        
        // Read and verify audio data
        auto audio_data = read_wav_audio_data(test_filename);
        REQUIRE(!audio_data.empty());
        
        // Should have the expected number of samples (1 second of audio)
        // Allow some tolerance for timing variations
        REQUIRE(audio_data.size() >= sample_rate * channels * 0.99);
        REQUIRE(audio_data.size() <= sample_rate * channels * 1.01);
        
        // Should contain the expected frequency
        REQUIRE(detect_frequency_int16(audio_data, frequency, sample_rate, channels));
        
        // Check that the amplitude is reasonable (not too quiet, not clipping)
        float rms = calculate_rms_int16(audio_data);
        REQUIRE(rms > 0.01f); // Not silence
        REQUIRE(rms < 0.5f);  // Not clipping
    }
}

TEST_CASE("AudioFileOutput error handling") {
    const unsigned frames_per_buffer = 512;
    const unsigned sample_rate = 44100;
    const unsigned channels = 2;
    
    SECTION("Try to start without opening") {
        AudioFileOutput file_output(frames_per_buffer, sample_rate, channels, "build/tests/test.wav");
        REQUIRE(file_output.start() == false);
    }
    
    SECTION("Try to stop without opening") {
        AudioFileOutput file_output(frames_per_buffer, sample_rate, channels, "build/tests/test.wav");
        REQUIRE(file_output.stop() == false);
    }
    
    SECTION("Try to close without opening") {
        AudioFileOutput file_output(frames_per_buffer, sample_rate, channels, "build/tests/test.wav");
        // The close() function will succeed even if file wasn't opened
        // because it just tries to close the file stream
        REQUIRE(file_output.close() == true);
    }
    
    SECTION("Try to push audio without starting") {
        AudioFileOutput file_output(frames_per_buffer, sample_rate, channels, "build/tests/test.wav");
        REQUIRE(file_output.open() == true);
        
        std::vector<float> buffer(frames_per_buffer * channels, 0.0f);
        file_output.push(buffer.data()); // Should not crash
        
        REQUIRE(file_output.close() == true);
    }
    
    SECTION("Try to write to invalid file path") {
        // Try to write to a directory that doesn't exist
        AudioFileOutput file_output(frames_per_buffer, sample_rate, channels, "/nonexistent/directory/test.wav");
        REQUIRE(file_output.open() == false);
    }
}

TEST_CASE("AudioFileOutput different configurations") {
    SECTION("Mono audio") {
        const unsigned frames_per_buffer = 256;
        const unsigned sample_rate = 48000;
        const unsigned channels = 1;
        const std::string test_filename = "build/tests/mono_test.wav";
        
        std::filesystem::remove(test_filename);
        
        AudioFileOutput file_output(frames_per_buffer, sample_rate, channels, test_filename);
        
        REQUIRE(file_output.open() == true);
        REQUIRE(file_output.start() == true);
        
        // Generate and write a short sine wave
        auto buffer = generate_sine_wave(440.0f, 0.2f, sample_rate, frames_per_buffer, channels);
        file_output.push(buffer.data());
        
        REQUIRE(file_output.stop() == true);
        REQUIRE(file_output.close() == true);
        
        // Verify file was created and has valid WAV header
        REQUIRE(validate_wav_header(test_filename, channels, sample_rate, 16));
        
        // Read and verify audio data
        auto audio_data = read_wav_audio_data(test_filename);
        REQUIRE(!audio_data.empty());
        REQUIRE(audio_data.size() == frames_per_buffer * channels);
    }
    
    SECTION("High sample rate") {
        const unsigned frames_per_buffer = 1024;
        const unsigned sample_rate = 96000;
        const unsigned channels = 2;
        const std::string test_filename = "build/tests/high_sample_rate_test.wav";
        
        std::filesystem::remove(test_filename);
        
        AudioFileOutput file_output(frames_per_buffer, sample_rate, channels, test_filename);
        
        REQUIRE(file_output.open() == true);
        REQUIRE(file_output.start() == true);
        
        // Generate and write a short sine wave
        auto buffer = generate_sine_wave(880.0f, 0.1f, sample_rate, frames_per_buffer, channels);
        file_output.push(buffer.data());
        
        REQUIRE(file_output.stop() == true);
        REQUIRE(file_output.close() == true);
        
        // Verify file was created and has valid WAV header
        REQUIRE(validate_wav_header(test_filename, channels, sample_rate, 16));
        
        // Read and verify audio data
        auto audio_data = read_wav_audio_data(test_filename);
        REQUIRE(!audio_data.empty());
        REQUIRE(audio_data.size() == frames_per_buffer * channels);
    }
    
    SECTION("Multi-channel audio (4 channels)") {
        const unsigned frames_per_buffer = 512;
        const unsigned sample_rate = 44100;
        const unsigned channels = 4;
        const std::string test_filename = "build/tests/multichannel_test.wav";
        
        std::filesystem::remove(test_filename);
        
        AudioFileOutput file_output(frames_per_buffer, sample_rate, channels, test_filename);
        
        REQUIRE(file_output.open() == true);
        REQUIRE(file_output.start() == true);
        
        // Generate and write a short sine wave
        auto buffer = generate_sine_wave(440.0f, 0.2f, sample_rate, frames_per_buffer, channels);
        file_output.push(buffer.data());
        
        REQUIRE(file_output.stop() == true);
        REQUIRE(file_output.close() == true);
        
        // Verify file was created and has valid WAV header
        REQUIRE(validate_wav_header(test_filename, channels, sample_rate, 16));
        
        // Read and verify audio data
        auto audio_data = read_wav_audio_data(test_filename);
        REQUIRE(!audio_data.empty());
        REQUIRE(audio_data.size() == frames_per_buffer * channels);
        
        // Verify that all channels contain data (not just zeros)
        bool has_non_zero_data = false;
        for (int16_t sample : audio_data) {
            if (sample != 0) {
                has_non_zero_data = true;
                break;
            }
        }
        REQUIRE(has_non_zero_data);
        
        // Test frequency detection on multiple channels
        for (unsigned ch = 0; ch < channels; ++ch) {
            REQUIRE(detect_frequency_int16_channel(audio_data, 440.0f, sample_rate, channels, ch));
        }
    }
    
    SECTION("Multi-channel audio (8 channels)") {
        const unsigned frames_per_buffer = 256;
        const unsigned sample_rate = 48000;
        const unsigned channels = 8;
        const std::string test_filename = "build/tests/octochannel_test.wav";
        
        std::filesystem::remove(test_filename);
        
        AudioFileOutput file_output(frames_per_buffer, sample_rate, channels, test_filename);
        
        REQUIRE(file_output.open() == true);
        REQUIRE(file_output.start() == true);
        
        // Generate and write a short sine wave
        auto buffer = generate_sine_wave(440.0f, 0.2f, sample_rate, frames_per_buffer, channels);
        file_output.push(buffer.data());
        
        REQUIRE(file_output.stop() == true);
        REQUIRE(file_output.close() == true);
        
        // Verify file was created and has valid WAV header
        REQUIRE(validate_wav_header(test_filename, channels, sample_rate, 16));
        
        // Read and verify audio data
        auto audio_data = read_wav_audio_data(test_filename);
        REQUIRE(!audio_data.empty());
        REQUIRE(audio_data.size() == frames_per_buffer * channels);
        
        // Verify that all channels contain data (not just zeros)
        bool has_non_zero_data = false;
        for (int16_t sample : audio_data) {
            if (sample != 0) {
                has_non_zero_data = true;
                break;
            }
        }
        REQUIRE(has_non_zero_data);
        
        // Test frequency detection on multiple channels
        for (unsigned ch = 0; ch < channels; ++ch) {
            REQUIRE(detect_frequency_int16_channel(audio_data, 440.0f, sample_rate, channels, ch));
        }
    }
}

TEST_CASE("AudioFileOutput continuous writing") {
    const unsigned frames_per_buffer = 512;
    const unsigned sample_rate = 44100;
    const unsigned channels = 2;
    const std::string test_filename = "build/tests/continuous_test.wav";
    
    // Clean up any existing test file
    std::filesystem::remove(test_filename);
    
    SECTION("Continuous sine wave with frequency sweep") {
        AudioFileOutput file_output(frames_per_buffer, sample_rate, channels, test_filename);
        
        REQUIRE(file_output.open() == true);
        REQUIRE(file_output.start() == true);
        
        // Write a frequency sweep for 2 seconds
        const unsigned num_buffers = (sample_rate / frames_per_buffer) * 2;
        float phase = 0.0f;
        
        for (unsigned i = 0; i < num_buffers; ++i) {
            // Wait until the file is ready for more audio
            // The is_ready() function has timing restrictions, so we need to be patient
            int wait_count = 0;
            while (!file_output.is_ready() && wait_count < 100) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                wait_count++;
            }
            // If still not ready after waiting, just proceed anyway
            // Calculate frequency that sweeps from 200Hz to 2000Hz
            float progress = static_cast<float>(i) / num_buffers;
            float frequency = 200.0f + (1800.0f * progress);
            // Generate sine wave buffer
            auto buffer = generate_sine_wave(frequency, 0.2f, sample_rate, 
                                           frames_per_buffer, channels, phase);
            // Write audio data
            file_output.push(buffer.data());
            // Update phase for next buffer
            phase += frames_per_buffer;
        }
        
        REQUIRE(file_output.stop() == true);
        REQUIRE(file_output.close() == true);
        
        // Verify file was created and has valid WAV header
        REQUIRE(validate_wav_header(test_filename, channels, sample_rate, 16));
        
        // Read and verify audio data
        auto audio_data = read_wav_audio_data(test_filename);
        REQUIRE(!audio_data.empty());
        
        // Should have the expected number of samples (2 seconds of audio)
        // Allow some tolerance for timing variations
        REQUIRE(audio_data.size() >= sample_rate * channels * 2 * 0.99);
        REQUIRE(audio_data.size() <= sample_rate * channels * 2 * 1.01);
        
        // Check that the amplitude is reasonable
        float rms = calculate_rms_int16(audio_data);
        REQUIRE(rms > 0.01f); // Not silence
        REQUIRE(rms < 0.5f);  // Not clipping
    }
}

TEST_CASE("AudioFileOutput data validation") {
    const unsigned frames_per_buffer = 256;
    const unsigned sample_rate = 44100;
    const unsigned channels = 2;
    const std::string test_filename = "build/tests/data_validation_test.wav";
    
    // Clean up any existing test file
    std::filesystem::remove(test_filename);
    
    SECTION("Verify constant value writing") {
        AudioFileOutput file_output(frames_per_buffer, sample_rate, channels, test_filename);
        
        REQUIRE(file_output.open() == true);
        REQUIRE(file_output.start() == true);
        
        // Write a constant value buffer
        float constant_value = 0.5f;
        auto buffer = generate_constant_buffer(constant_value, frames_per_buffer, channels);
        file_output.push(buffer.data());
        
        REQUIRE(file_output.stop() == true);
        REQUIRE(file_output.close() == true);
        
        // Read and verify audio data
        auto audio_data = read_wav_audio_data(test_filename);
        REQUIRE(!audio_data.empty());
        REQUIRE(audio_data.size() == frames_per_buffer * channels);
        
        // Convert expected value to int16_t
        int16_t expected_sample = float_to_int16(constant_value);
        
        // Check that all samples match the expected value
        for (int16_t sample : audio_data) {
            REQUIRE(sample == expected_sample);
        }
    }
    
    SECTION("Verify amplitude scaling") {
        AudioFileOutput file_output(frames_per_buffer, sample_rate, channels, test_filename);
        
        REQUIRE(file_output.open() == true);
        REQUIRE(file_output.start() == true);
        
        // Write a sine wave with known amplitude
        float amplitude = 0.8f;
        auto buffer = generate_sine_wave(440.0f, amplitude, sample_rate, frames_per_buffer, channels);
        file_output.push(buffer.data());
        
        REQUIRE(file_output.stop() == true);
        REQUIRE(file_output.close() == true);
        
        // Read and verify audio data
        auto audio_data = read_wav_audio_data(test_filename);
        REQUIRE(!audio_data.empty());
        
        // Find the peak amplitude
        int16_t max_amplitude = 0;
        for (int16_t sample : audio_data) {
            max_amplitude = std::max(max_amplitude, static_cast<int16_t>(std::abs(sample)));
        }
        
        // Convert expected amplitude to int16_t
        int16_t expected_max = float_to_int16(amplitude);
        
        // Allow some tolerance for rounding errors
        REQUIRE(std::abs(max_amplitude - expected_max) <= 1);
    }
    
    SECTION("Verify channel separation") {
        AudioFileOutput file_output(frames_per_buffer, sample_rate, channels, test_filename);
        
        REQUIRE(file_output.open() == true);
        REQUIRE(file_output.start() == true);
        
        // Create a buffer where left channel is 0.5 and right channel is -0.5
        std::vector<float> buffer(frames_per_buffer * channels);
        for (unsigned i = 0; i < frames_per_buffer; ++i) {
            buffer[i * channels + 0] = 0.5f;  // Left channel
            buffer[i * channels + 1] = -0.5f; // Right channel
        }
        
        file_output.push(buffer.data());
        
        REQUIRE(file_output.stop() == true);
        REQUIRE(file_output.close() == true);
        
        // Read and verify audio data
        auto audio_data = read_wav_audio_data(test_filename);
        REQUIRE(!audio_data.empty());
        REQUIRE(audio_data.size() == frames_per_buffer * channels);
        
        // Convert expected values to int16_t
        int16_t expected_left = float_to_int16(0.5f);
        int16_t expected_right = float_to_int16(-0.5f);
        
        // Check that channels are correctly separated
        for (unsigned i = 0; i < frames_per_buffer; ++i) {
            REQUIRE(audio_data[i * channels + 0] == expected_left);   // Left channel
            REQUIRE(audio_data[i * channels + 1] == expected_right);  // Right channel
        }
    }
}

TEST_CASE("AudioFileOutput file size validation") {
    const unsigned frames_per_buffer = 512;
    const unsigned sample_rate = 44100;
    const unsigned channels = 2;
    const std::string test_filename = "build/tests/file_size_test.wav";
    
    // Clean up any existing test file
    std::filesystem::remove(test_filename);
    
    SECTION("Verify file size matches expected data size") {
        AudioFileOutput file_output(frames_per_buffer, sample_rate, channels, test_filename);
        
        REQUIRE(file_output.open() == true);
        REQUIRE(file_output.start() == true);
        
        // Write multiple buffers
        const unsigned num_buffers = 10;
        auto buffer = generate_sine_wave(440.0f, 0.3f, sample_rate, frames_per_buffer, channels);
        
        for (unsigned i = 0; i < num_buffers; ++i) {
            file_output.push(buffer.data());
        }
        
        REQUIRE(file_output.stop() == true);
        REQUIRE(file_output.close() == true);
        
        // Calculate expected file size
        size_t expected_data_size = num_buffers * frames_per_buffer * channels * sizeof(int16_t);
        size_t expected_file_size = sizeof(WAVHeader) + expected_data_size;
        
        // Check actual file size
        std::filesystem::path file_path(test_filename);
        REQUIRE(std::filesystem::exists(file_path));
        REQUIRE(std::filesystem::file_size(file_path) == expected_file_size);
        
        // Verify WAV header data size field
        std::ifstream file(test_filename, std::ios::binary);
        WAVHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));
        file.close();
        
        REQUIRE(header.data_size == expected_data_size);
    }
} 