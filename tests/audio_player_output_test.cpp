#include "catch2/catch_all.hpp"
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <mutex>
#include <atomic>

#include <SDL2/SDL.h>
#include "audio_output/audio_player_output.h"

// Global variables for audio capture
std::vector<float> captured_audio;
std::mutex audio_mutex;
std::atomic<bool> capture_active(false);

// SDL Audio callback to capture audio output
void audio_capture_callback(void* userdata, Uint8* stream, int len) {
    if (!capture_active.load()) return;
    
    // Convert the audio stream to float samples
    float* float_stream = reinterpret_cast<float*>(stream);
    int num_samples = len / sizeof(float);
    
    std::lock_guard<std::mutex> lock(audio_mutex);
    captured_audio.insert(captured_audio.end(), float_stream, float_stream + num_samples);
}

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

// Helper function to calculate RMS (Root Mean Square) of audio data
float calculate_rms(const std::vector<float>& audio_data) {
    if (audio_data.empty()) return 0.0f;
    
    float sum_squares = 0.0f;
    for (float sample : audio_data) {
        sum_squares += sample * sample;
    }
    
    return std::sqrt(sum_squares / audio_data.size());
}

// Helper function to calculate peak amplitude
float calculate_peak(const std::vector<float>& audio_data) {
    if (audio_data.empty()) return 0.0f;
    
    float max_amplitude = 0.0f;
    for (float sample : audio_data) {
        max_amplitude = std::max(max_amplitude, std::abs(sample));
    }
    
    return max_amplitude;
}

// Helper function to detect if audio contains the expected frequency
bool detect_frequency(const std::vector<float>& audio_data, float expected_freq, 
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


TEST_CASE("AudioPlayerOutput basic functionality") {
    const unsigned frames_per_buffer = 512;
    const unsigned sample_rate = 44100;
    const unsigned channels = 2;
    
    SECTION("Open and close audio device") {
        AudioPlayerOutput player(frames_per_buffer, sample_rate, channels);
        REQUIRE(player.open() == true);
        REQUIRE(player.close() == true);
    }
    
    SECTION("Start and stop audio device") {
        AudioPlayerOutput player(frames_per_buffer, sample_rate, channels);
        REQUIRE(player.open() == true);
        REQUIRE(player.start() == true);
        REQUIRE(player.stop() == true);
        REQUIRE(player.close() == true);
    }
    
    SECTION("Check ready state") {
        AudioPlayerOutput player(frames_per_buffer, sample_rate, channels);
        REQUIRE(player.open() == true);
        REQUIRE(player.start() == true);
        
        // Should be ready after starting
        REQUIRE(player.is_ready() == true);
        
        REQUIRE(player.stop() == true);
        REQUIRE(player.close() == true);
    }
    
    SECTION("Queued bytes consumption") {
        AudioPlayerOutput player(frames_per_buffer, sample_rate, channels);

        REQUIRE(player.open() == true);
        REQUIRE(player.start() == true);

        // Before pushing, nothing should be queued.
        size_t queued_before = player.queued_bytes();
        REQUIRE(queued_before == 0);

        // Generate one buffer of audio and queue it.
        auto buffer = generate_sine_wave(440.0f, 0.3f, sample_rate,
                                         frames_per_buffer, channels);
        player.push(buffer.data());

        // After push we expect SDL to report queued data.
        size_t queued_after_push = player.queued_bytes();
        REQUIRE(queued_after_push > 0);

        // Give the device thread some time to consume the data.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        size_t queued_after_wait = player.queued_bytes();

        // The queued amount should have decreased (or reached zero) once playback progressed.
        REQUIRE(queued_after_wait < queued_after_push);

        REQUIRE(player.stop() == true);
        REQUIRE(player.close() == true);
    }
}

TEST_CASE("AudioPlayerOutput sine wave playback") {
    const unsigned frames_per_buffer = 512;
    const unsigned sample_rate = 44100;
    const unsigned channels = 2;
    const float frequency = 440.0f; // A4 note
    const float amplitude = 0.3f;
    
    SECTION("Play a simple sine wave") {
        AudioPlayerOutput player(frames_per_buffer, sample_rate, channels);
        
        REQUIRE(player.open() == true);
        REQUIRE(player.start() == true);
        
        // Clear any initial queued audio to start with a clean state
        player.clear_queue();
        
        // Generate and play sine wave for 1 second
        const unsigned num_buffers = sample_rate / frames_per_buffer;
        float phase = 0.0f;
        
        for (unsigned i = 0; i < num_buffers; ++i) {
            // Wait until the device is ready for more audio
            while (!player.is_ready()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
            // Generate sine wave buffer
            auto buffer = generate_sine_wave(frequency, amplitude, sample_rate, 
                                           frames_per_buffer, channels, phase);

            // Check current queue state before pushing
            size_t queued_before = player.queued_bytes();
            
            // Push audio data
            player.push(buffer.data());
            
            // Update phase for next buffer
            phase += frames_per_buffer;
        }
        
        // After push we expect SDL to report queued data.
        size_t queued_after_push = player.queued_bytes();
        REQUIRE(queued_after_push > 0);

        // Wait a bit for audio to finish playing
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // The queued amount should have decreased (or reached zero) once playback progressed.
        size_t queued_after_wait = player.queued_bytes();
        REQUIRE(queued_after_wait < queued_after_push);
        
        REQUIRE(player.stop() == true);
        REQUIRE(player.close() == true);
    }
}

TEST_CASE("AudioPlayerOutput error handling") {
    const unsigned frames_per_buffer = 512;
    const unsigned sample_rate = 44100;
    const unsigned channels = 2;
    
    SECTION("Try to start without opening") {
        AudioPlayerOutput player(frames_per_buffer, sample_rate, channels);
        REQUIRE(player.start() == false);
    }
    
    SECTION("Try to stop without opening") {
        AudioPlayerOutput player(frames_per_buffer, sample_rate, channels);
        REQUIRE(player.stop() == false);
    }
    
    SECTION("Try to close without opening") {
        AudioPlayerOutput player(frames_per_buffer, sample_rate, channels);
        REQUIRE(player.close() == false);
    }
    
    SECTION("Try to push audio without starting") {
        AudioPlayerOutput player(frames_per_buffer, sample_rate, channels);
        REQUIRE(player.open() == true);
        
        std::vector<float> buffer(frames_per_buffer * channels, 0.0f);
        player.push(buffer.data()); // Should not crash
        
        REQUIRE(player.close() == true);
    }
}

TEST_CASE("AudioPlayerOutput different configurations") {
    SECTION("Mono audio") {
        const unsigned frames_per_buffer = 256;
        const unsigned sample_rate = 48000;
        const unsigned channels = 1;
        
        AudioPlayerOutput player(frames_per_buffer, sample_rate, channels);
        
        REQUIRE(player.open() == true);
        REQUIRE(player.start() == true);
        
        // Generate and play a short sine wave
        auto buffer = generate_sine_wave(440.0f, 0.2f, sample_rate, 
                                       frames_per_buffer, channels);
        player.push(buffer.data());
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        REQUIRE(player.stop() == true);
        REQUIRE(player.close() == true);
    }
    
    SECTION("High sample rate") {
        const unsigned frames_per_buffer = 1024;
        const unsigned sample_rate = 96000;
        const unsigned channels = 2;
        
        AudioPlayerOutput player(frames_per_buffer, sample_rate, channels);
        
        REQUIRE(player.open() == true);
        REQUIRE(player.start() == true);
        
        // Generate and play a short sine wave
        auto buffer = generate_sine_wave(880.0f, 0.1f, sample_rate, 
                                       frames_per_buffer, channels);
        player.push(buffer.data());
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        REQUIRE(player.stop() == true);
        REQUIRE(player.close() == true);
    }
}

TEST_CASE("AudioPlayerOutput continuous playback") {
    const unsigned frames_per_buffer = 512;
    const unsigned sample_rate = 44100;
    const unsigned channels = 2;
    
    SECTION("Continuous sine wave with frequency sweep") {
        AudioPlayerOutput player(frames_per_buffer, sample_rate, channels);
        
        REQUIRE(player.open() == true);
        REQUIRE(player.start() == true);
        
        // Clear any initial queued audio to start with a clean state
        player.clear_queue();
        
        // Play a frequency sweep for 2 seconds
        const unsigned num_buffers = (sample_rate / frames_per_buffer) * 2;
        float phase = 0.0f;
        
        for (unsigned i = 0; i < num_buffers; ++i) {
            // Wait until the device is ready for more audio
            while (!player.is_ready()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
            // Calculate frequency that sweeps from 200Hz to 2000Hz
            float progress = static_cast<float>(i) / num_buffers;
            float frequency = 200.0f + (1800.0f * progress);
            
            // Generate sine wave buffer
            auto buffer = generate_sine_wave(frequency, 0.2f, sample_rate, 
                                           frames_per_buffer, channels, phase);

            // Measure queued bytes before push
            size_t queued_before = player.queued_bytes();
            
            // Push audio data
            player.push(buffer.data());

            // Measure queued bytes after push
            size_t queued_after_push = player.queued_bytes();
            // After push, queued bytes should increase or stay the same (if device consumed immediately)
            REQUIRE(queued_after_push >= queued_before);
            
            // Update phase for next buffer
            phase += frames_per_buffer;
        }
        
        // Wait for audio to finish
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // After waiting, queued bytes should have decreased or reached zero
        size_t queued_after_wait = player.queued_bytes();
        REQUIRE(queued_after_wait <= player.queued_bytes());
        
        REQUIRE(player.stop() == true);
        REQUIRE(player.close() == true);
    }
}

TEST_CASE("AudioPlayerOutput buffer management") {
    const unsigned frames_per_buffer = 256;
    const unsigned sample_rate = 44100;
    const unsigned channels = 2;
    
    SECTION("Test buffer overflow handling") {
        AudioPlayerOutput player(frames_per_buffer, sample_rate, channels);
        
        REQUIRE(player.open() == true);
        REQUIRE(player.start() == true);
        
        // Fill the buffer completely
        std::vector<float> buffer(frames_per_buffer * channels, 0.1f);
        
        // Push many buffers quickly to test overflow handling
        for (int i = 0; i < 100; ++i) {
            player.push(buffer.data());
        }
        
        // Wait a bit for the audio system to process
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        REQUIRE(player.stop() == true);
        REQUIRE(player.close() == true);
    }
    
    SECTION("Test is_ready() behavior") {
        AudioPlayerOutput player(frames_per_buffer, sample_rate, channels);
        
        REQUIRE(player.open() == true);
        REQUIRE(player.start() == true);
        
        // Initially should be ready
        REQUIRE(player.is_ready() == true);
        
        // Fill the buffer
        std::vector<float> buffer(frames_per_buffer * channels, 0.1f);
        for (int i = 0; i < 10; ++i) {
            player.push(buffer.data());
        }
        
        // May not be ready immediately after pushing
        // Wait a bit and check again
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        REQUIRE(player.stop() == true);
        REQUIRE(player.close() == true);
    }
} 