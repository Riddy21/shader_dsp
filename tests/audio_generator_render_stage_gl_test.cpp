#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"
#include "utilities/egl_compatibility.h"

#include "audio_core/audio_render_stage.h"
#include "audio_core/audio_render_graph.h"
#include "audio_core/audio_renderer.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_render_stage/audio_file_generator_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_parameter/audio_texture2d_parameter.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"
#include "audio_output/audio_player_output.h"
#include "audio_output/audio_wav.h"
#include "engine/event_loop.h"

#include <functional>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>

/**
 * @brief Tests for generator render stage functionality with OpenGL context
 * 
 * These tests check the generator render stage creation, initialization, and rendering in an OpenGL context.
 * Focus on sine wave generation with comprehensive waveform analysis and glitch detection.
 * Note: These tests require a valid OpenGL context to run, which may not be available
 * in all test environments. They are marked with [gl] tag.
 */
constexpr int WIDTH = 512;
constexpr int HEIGHT = 2;

// Helper function to load original audio data from WAV file
std::vector<std::vector<float>> load_original_audio_data(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open audio file: " + filename);
    }

    WAVHeader header;
    file.read((char*)&header, sizeof(WAVHeader));

    if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0) {
        throw std::runtime_error("Invalid audio file format: " + filename);
    }

    if (header.format_type != 1) {
        throw std::runtime_error("Invalid audio file format type: " + filename);
    }

    // Read the audio data
    std::vector<int16_t> data(header.data_size / sizeof(int16_t));
    file.read((char*)data.data(), header.data_size);

    if (!file) {
        throw std::runtime_error("Failed to read audio data from file: " + filename);
    }

    // Convert to float and separate channels
    std::vector<std::vector<float>> audio_data(header.channels, std::vector<float>(data.size() / header.channels));
    for (unsigned int i = 0; i < data.size(); i++) {
        audio_data[i % header.channels][i / header.channels] = data[i] / 32768.0f;
    }

    return audio_data;
}

// Helper function to calculate correlation between two audio samples
float calculate_correlation(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) {
        return 0.0f;
    }

    float sum_a = 0.0f, sum_b = 0.0f, sum_ab = 0.0f, sum_a2 = 0.0f, sum_b2 = 0.0f;
    
    for (size_t i = 0; i < a.size(); ++i) {
        sum_a += a[i];
        sum_b += b[i];
        sum_ab += a[i] * b[i];
        sum_a2 += a[i] * a[i];
        sum_b2 += b[i] * b[i];
    }

    float n = static_cast<float>(a.size());
    float numerator = n * sum_ab - sum_a * sum_b;
    float denominator = std::sqrt((n * sum_a2 - sum_a * sum_a) * (n * sum_b2 - sum_b * sum_b));
    
    return denominator != 0.0f ? numerator / denominator : 0.0f;
}

// Helper function to calculate RMS error between two audio samples
float calculate_rms_error(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) {
        return std::numeric_limits<float>::infinity();
    }

    float sum_squared_error = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        float error = a[i] - b[i];
        sum_squared_error += error * error;
    }

    return std::sqrt(sum_squared_error / static_cast<float>(a.size()));
}

TEST_CASE("AudioGeneratorRenderStage - Sine Wave Generation", "[audio_generator_render_stage][gl_test]") {
    SDLWindow window(WIDTH, HEIGHT);
    GLContext context;

    constexpr int BUFFER_SIZE = WIDTH;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_CHANNELS = HEIGHT;
    constexpr float TEST_FREQUENCY = 450.0f; // A4 note
    constexpr float TEST_GAIN = 0.3f;
    constexpr int NUM_FRAMES = SAMPLE_RATE / BUFFER_SIZE * 5; // 5 seconds

    // Create sine generator render stage
    AudioGeneratorRenderStage sine_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 
                                            "build/shaders/multinote_sine_generator_render_stage.glsl");

    AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Connect the generator to the final render stage
    REQUIRE(sine_generator.connect_render_stage(&final_render_stage));
    
    // Add global_time parameter as a buffer parameter
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();

    // Remove the envelope for clean sine wave
    auto attack_param = sine_generator.find_parameter("attack_time");
    attack_param->set_value(0.0f);
    auto decay_param = sine_generator.find_parameter("decay_time");
    decay_param->set_value(0.0f);
    auto sustain_param = sine_generator.find_parameter("sustain_level");
    sustain_param->set_value(1.0f);
    auto release_param = sine_generator.find_parameter("release_time");
    release_param->set_value(0.0f);
    
    // Initialize the render stages
    REQUIRE(sine_generator.initialize());
    REQUIRE(final_render_stage.initialize());

    context.prepare_draw();
    // Bind the render stages
    REQUIRE(sine_generator.bind());
    REQUIRE(final_render_stage.bind());

    // Play a note
    sine_generator.play_note(TEST_FREQUENCY, TEST_GAIN);

    // Render multiple frames to test continuity
    std::vector<float> left_channel_samples;
    std::vector<float> right_channel_samples;
    left_channel_samples.reserve(BUFFER_SIZE * NUM_FRAMES);
    right_channel_samples.reserve(BUFFER_SIZE * NUM_FRAMES);

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        // Set global_time for this frame
        global_time_param->set_value(frame);
        global_time_param->render();

        // Render the generator stage
        sine_generator.render(frame);
        
        // Render the final stage for interpolation
        final_render_stage.render(frame);
        
        // Get the final output audio data from the final render stage
        auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
        REQUIRE(output_param != nullptr);

        const float* output_data = static_cast<const float*>(output_param->get_value());
        REQUIRE(output_data != nullptr);

        // Store samples for analysis - separate left and right channels
        for (int i = 0; i < BUFFER_SIZE; i++) {
            left_channel_samples.push_back(output_data[i * NUM_CHANNELS]);
            right_channel_samples.push_back(output_data[i * NUM_CHANNELS + 1]);
        }
    }

    // Analyze the waveform for exact sine wave characteristics
    REQUIRE(left_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);
    REQUIRE(right_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);

    SECTION("Frequency Accuracy") {
        // Test both channels
        std::vector<std::vector<float>*> channels = {&left_channel_samples, &right_channel_samples};
        std::vector<std::string> channel_names = {"Left", "Right"};
        
        for (size_t ch = 0; ch < channels.size(); ch++) {
            const auto& samples = *channels[ch];
            const std::string& name = channel_names[ch];
            
            INFO("Testing " << name << " channel");
            
            // Find zero crossings to verify frequency
            std::vector<int> zero_crossings;
            for (size_t i = 1; i < samples.size(); ++i) {
                if ((samples[i-1] < 0 && samples[i] >= 0) || 
                    (samples[i-1] > 0 && samples[i] <= 0)) {
                    zero_crossings.push_back(i);
                }
            }

            REQUIRE(zero_crossings.size() >= 2); // Should have multiple zero crossings

            // Calculate measured frequency from zero crossings
            if (zero_crossings.size() >= 2) {
                float total_time = static_cast<float>(samples.size()) / SAMPLE_RATE;
                float measured_frequency = static_cast<float>(zero_crossings.size() - 1) / (2.0f * total_time);
                
                // Allow very small tolerance for frequency measurement
                REQUIRE(measured_frequency == Catch::Approx(TEST_FREQUENCY).margin(1.0f));
            }
        }
    }

    SECTION("Amplitude and Waveform Characteristics") {
        // Test both channels
        std::vector<std::vector<float>*> channels = {&left_channel_samples, &right_channel_samples};
        std::vector<std::string> channel_names = {"Left", "Right"};
        
        for (size_t ch = 0; ch < channels.size(); ch++) {
            const auto& samples = *channels[ch];
            const std::string& name = channel_names[ch];
            
            INFO("Testing " << name << " channel");
            
            // Test amplitude
            {
                float max_amplitude = 0.0f;
                for (float sample : samples) {
                    max_amplitude = std::max(max_amplitude, std::abs(sample));
                }

                // Should be exactly TEST_GAIN
                REQUIRE(max_amplitude == Catch::Approx(TEST_GAIN).margin(0.01f));
            }

            // Test RMS
            {
                // Calculate RMS and verify it matches expected sine wave RMS
                float rms = 0.0f;
                for (float sample : samples) {
                    rms += sample * sample;
                }
                rms = std::sqrt(rms / samples.size());
                
                // For a sine wave: RMS = amplitude / sqrt(2)
                float expected_rms = TEST_GAIN / std::sqrt(2.0f);
                REQUIRE(rms == Catch::Approx(expected_rms).margin(0.01f));
            }

            // Test DC offset
            {
                float sum = 0.0f;
                for (float sample : samples) {
                    sum += sample;
                }
                float dc_offset = sum / samples.size();
                
                // DC offset should be exactly zero for a perfect sine wave
                REQUIRE(std::abs(dc_offset) < 0.001f);
            }
        }
    }

    SECTION("Continuity and Glitch Detection") {
        // Test both channels
        std::vector<std::vector<float>*> channels = {&left_channel_samples, &right_channel_samples};
        std::vector<std::string> channel_names = {"Left", "Right"};
        
        for (size_t ch = 0; ch < channels.size(); ch++) {
            const auto& samples = *channels[ch];
            const std::string& name = channel_names[ch];
            
            INFO("Testing " << name << " channel");
            
            // Test sample continuity
            {
                constexpr float MAX_SAMPLE_DIFF = 0.02f; // Very strict tolerance for exact sine wave
                
                for (size_t i = 1; i < samples.size(); ++i) {
                    float sample_diff = std::abs(samples[i] - samples[i-1]);
                    
                    // Check for sudden jumps that would indicate glitches
                    REQUIRE(sample_diff <= MAX_SAMPLE_DIFF);
                }
            }

            // Test phase continuity
            {
                // Calculate expected phase increment per sample
                float phase_increment = 2.0f * M_PI * TEST_FREQUENCY / SAMPLE_RATE;
                
                // Check for phase continuity by analyzing consecutive samples
                for (size_t i = 1; i < samples.size(); ++i) {
                    // For a continuous sine wave, consecutive samples should follow the phase increment
                    // We can estimate the phase difference from the amplitude difference
                    float expected_diff = TEST_GAIN * 2.0f * M_PI * TEST_FREQUENCY / SAMPLE_RATE;
                    
                    // The actual difference should be within reasonable bounds
                    float actual_diff = std::abs(samples[i] - samples[i-1]);
                    REQUIRE(actual_diff <= expected_diff * 2.0f); // Allow some tolerance
                }
            }
        }
    }

    SECTION("Data Quality Validation") {
        // Test both channels
        std::vector<std::vector<float>*> channels = {&left_channel_samples, &right_channel_samples};
        std::vector<std::string> channel_names = {"Left", "Right"};
        
        for (size_t ch = 0; ch < channels.size(); ch++) {
            const auto& samples = *channels[ch];
            const std::string& name = channel_names[ch];
            
            INFO("Testing " << name << " channel");
            
            // Test for NaN and infinite values
            {
                for (float sample : samples) {
                    REQUIRE(!std::isnan(sample));
                    REQUIRE(!std::isinf(sample));
                }
            }

            // Test for clipping
            {
                for (float sample : samples) {
                    // Samples should not exceed the gain value
                    REQUIRE(std::abs(sample) <= TEST_GAIN);
                }
            }
        }
    }

    SECTION("Channel Correlation") {
        // Both channels should be identical for a mono sine wave
        REQUIRE(left_channel_samples.size() == right_channel_samples.size());
        
        // Check that both channels are identical
        for (size_t i = 0; i < left_channel_samples.size(); ++i) {
            REQUIRE(left_channel_samples[i] == Catch::Approx(right_channel_samples[i]).margin(0.001f));
        }
    }

    final_render_stage.unbind();
    sine_generator.unbind();
    delete global_time_param;
}

TEST_CASE("AudioFileGeneratorRenderStage - File Playback Test", "[audio_file_generator_render_stage][gl_test]") {
    SDLWindow window(WIDTH, HEIGHT);
    GLContext context;

    constexpr int BUFFER_SIZE = WIDTH;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_CHANNELS = HEIGHT;
    constexpr float TEST_GAIN = 0.5f;
    constexpr int NUM_FRAMES = SAMPLE_RATE / BUFFER_SIZE * 3; // 3 seconds

    // Test file path
    const std::string test_file_path = "media/test.wav";

    SECTION("Normal Speed Playback (1.0x)") {
        // Create file generator render stage
        AudioFileGeneratorRenderStage file_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, test_file_path);
        AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

        // Connect the generator to the final render stage
        REQUIRE(file_generator.connect_render_stage(&final_render_stage));
        
        // Add global_time parameter as a buffer parameter
        auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
        global_time_param->set_value(0);
        global_time_param->initialize();

        // Remove the envelope for clean playback
        auto attack_param = file_generator.find_parameter("attack_time");
        attack_param->set_value(0.0f);
        auto decay_param = file_generator.find_parameter("decay_time");
        decay_param->set_value(0.0f);
        auto sustain_param = file_generator.find_parameter("sustain_level");
        sustain_param->set_value(1.0f);
        auto release_param = file_generator.find_parameter("release_time");
        release_param->set_value(0.0f);
        
        // Initialize the render stages
        REQUIRE(file_generator.initialize());
        REQUIRE(final_render_stage.initialize());

        context.prepare_draw();
        REQUIRE(file_generator.bind());
        REQUIRE(final_render_stage.bind());

        // Play a note at normal speed (1.0 = MIDDLE_C)
        file_generator.play_note(MIDDLE_C, TEST_GAIN);

        // Render frames and collect samples
        std::vector<float> left_channel_samples;
        std::vector<float> right_channel_samples;
        left_channel_samples.reserve(BUFFER_SIZE * NUM_FRAMES);
        right_channel_samples.reserve(BUFFER_SIZE * NUM_FRAMES);

        for (int frame = 0; frame < NUM_FRAMES; frame++) {
            global_time_param->set_value(frame);
            global_time_param->render();

            file_generator.render(frame);
            final_render_stage.render(frame);
            
            auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
            REQUIRE(output_param != nullptr);

            const float* output_data = static_cast<const float*>(output_param->get_value());
            REQUIRE(output_data != nullptr);

            for (int i = 0; i < BUFFER_SIZE; i++) {
                left_channel_samples.push_back(output_data[i * NUM_CHANNELS]);
                right_channel_samples.push_back(output_data[i * NUM_CHANNELS + 1]);
            }
        }

        // Test basic audio characteristics
        REQUIRE(left_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);
        REQUIRE(right_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);

        // Test that we have non-zero audio data
        {
            float max_amplitude = 0.0f;
            for (float sample : left_channel_samples) {
                max_amplitude = std::max(max_amplitude, std::abs(sample));
            }
            REQUIRE(max_amplitude > 0.0f); // Should have some audio content
        }

        // Test for data quality
        {
            for (float sample : left_channel_samples) {
                REQUIRE(!std::isnan(sample));
                REQUIRE(!std::isinf(sample));
                REQUIRE(std::abs(sample) <= 1.0f); // Should not clip
            }
        }

        final_render_stage.unbind();
        file_generator.unbind();
        delete global_time_param;
    }

    SECTION("Half Speed Playback (0.5x)") {
        // Create file generator render stage
        AudioFileGeneratorRenderStage file_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, test_file_path);
        AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

        REQUIRE(file_generator.connect_render_stage(&final_render_stage));
        
        auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
        global_time_param->set_value(0);
        global_time_param->initialize();

        // Remove the envelope
        auto attack_param = file_generator.find_parameter("attack_time");
        attack_param->set_value(0.0f);
        auto decay_param = file_generator.find_parameter("decay_time");
        decay_param->set_value(0.0f);
        auto sustain_param = file_generator.find_parameter("sustain_level");
        sustain_param->set_value(1.0f);
        auto release_param = file_generator.find_parameter("release_time");
        release_param->set_value(0.0f);
        
        REQUIRE(file_generator.initialize());
        REQUIRE(final_render_stage.initialize());

        context.prepare_draw();
        REQUIRE(file_generator.bind());
        REQUIRE(final_render_stage.bind());

        // Play at half speed (0.5 * MIDDLE_C)
        file_generator.play_note(MIDDLE_C * 0.5f, TEST_GAIN);

        std::vector<float> left_channel_samples;
        std::vector<float> right_channel_samples;
        left_channel_samples.reserve(BUFFER_SIZE * NUM_FRAMES);
        right_channel_samples.reserve(BUFFER_SIZE * NUM_FRAMES);

        for (int frame = 0; frame < NUM_FRAMES; frame++) {
            global_time_param->set_value(frame);
            global_time_param->render();

            file_generator.render(frame);
            final_render_stage.render(frame);
            
            auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
            REQUIRE(output_param != nullptr);

            const float* output_data = static_cast<const float*>(output_param->get_value());
            REQUIRE(output_data != nullptr);

            for (int i = 0; i < BUFFER_SIZE; i++) {
                left_channel_samples.push_back(output_data[i * NUM_CHANNELS]);
                right_channel_samples.push_back(output_data[i * NUM_CHANNELS + 1]);
            }
        }

        REQUIRE(left_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);
        REQUIRE(right_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);

        // Test that we have non-zero audio data
        {
            float max_amplitude = 0.0f;
            for (float sample : left_channel_samples) {
                max_amplitude = std::max(max_amplitude, std::abs(sample));
            }
            REQUIRE(max_amplitude > 0.0f);
        }

        final_render_stage.unbind();
        file_generator.unbind();
        delete global_time_param;
    }

    SECTION("Double Speed Playback (2.0x)") {
        // Create file generator render stage
        AudioFileGeneratorRenderStage file_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, test_file_path);
        AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

        REQUIRE(file_generator.connect_render_stage(&final_render_stage));
        
        auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
        global_time_param->set_value(0);
        global_time_param->initialize();

        // Remove the envelope
        auto attack_param = file_generator.find_parameter("attack_time");
        attack_param->set_value(0.0f);
        auto decay_param = file_generator.find_parameter("decay_time");
        decay_param->set_value(0.0f);
        auto sustain_param = file_generator.find_parameter("sustain_level");
        sustain_param->set_value(1.0f);
        auto release_param = file_generator.find_parameter("release_time");
        release_param->set_value(0.0f);
        
        REQUIRE(file_generator.initialize());
        REQUIRE(final_render_stage.initialize());

        context.prepare_draw();
        REQUIRE(file_generator.bind());
        REQUIRE(final_render_stage.bind());

        // Play at double speed (2.0 * MIDDLE_C)
        file_generator.play_note(MIDDLE_C * 2.0f, TEST_GAIN);

        std::vector<float> left_channel_samples;
        std::vector<float> right_channel_samples;
        left_channel_samples.reserve(BUFFER_SIZE * NUM_FRAMES);
        right_channel_samples.reserve(BUFFER_SIZE * NUM_FRAMES);

        for (int frame = 0; frame < NUM_FRAMES; frame++) {
            global_time_param->set_value(frame);
            global_time_param->render();

            file_generator.render(frame);
            final_render_stage.render(frame);
            
            auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
            REQUIRE(output_param != nullptr);

            const float* output_data = static_cast<const float*>(output_param->get_value());
            REQUIRE(output_data != nullptr);

            for (int i = 0; i < BUFFER_SIZE; i++) {
                left_channel_samples.push_back(output_data[i * NUM_CHANNELS]);
                right_channel_samples.push_back(output_data[i * NUM_CHANNELS + 1]);
            }
        }

        REQUIRE(left_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);
        REQUIRE(right_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);

        // Test that we have non-zero audio data
        {
            float max_amplitude = 0.0f;
            for (float sample : left_channel_samples) {
                max_amplitude = std::max(max_amplitude, std::abs(sample));
            }
            REQUIRE(max_amplitude > 0.0f);
        }

        final_render_stage.unbind();
        file_generator.unbind();
        delete global_time_param;
    }

    SECTION("Speed Comparison Test") {
        // Test that different speeds produce different audio characteristics
        std::vector<float> normal_speed_samples, half_speed_samples, double_speed_samples;
        
        // Helper function to render audio at a specific speed
        auto render_at_speed = [&](float speed, std::vector<float>& samples) {
            AudioFileGeneratorRenderStage file_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, test_file_path);
            AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

            REQUIRE(file_generator.connect_render_stage(&final_render_stage));
            
            auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
            global_time_param->set_value(0);
            global_time_param->initialize();

            // Remove the envelope
            auto attack_param = file_generator.find_parameter("attack_time");
            attack_param->set_value(0.0f);
            auto decay_param = file_generator.find_parameter("decay_time");
            decay_param->set_value(0.0f);
            auto sustain_param = file_generator.find_parameter("sustain_level");
            sustain_param->set_value(1.0f);
            auto release_param = file_generator.find_parameter("release_time");
            release_param->set_value(0.0f);
            
            REQUIRE(file_generator.initialize());
            REQUIRE(final_render_stage.initialize());

            context.prepare_draw();
            REQUIRE(file_generator.bind());
            REQUIRE(final_render_stage.bind());

            file_generator.play_note(MIDDLE_C * speed, TEST_GAIN);

            samples.reserve(BUFFER_SIZE * NUM_FRAMES);

            for (int frame = 0; frame < NUM_FRAMES; frame++) {
                global_time_param->set_value(frame);
                global_time_param->render();

                file_generator.render(frame);
                final_render_stage.render(frame);
                
                auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
                REQUIRE(output_param != nullptr);

                const float* output_data = static_cast<const float*>(output_param->get_value());
                REQUIRE(output_data != nullptr);

                for (int i = 0; i < BUFFER_SIZE; i++) {
                    samples.push_back(output_data[i * NUM_CHANNELS]);
                }
            }

            final_render_stage.unbind();
            file_generator.unbind();
            delete global_time_param;
        };

        // Render at different speeds
        render_at_speed(1.0f, normal_speed_samples);
        render_at_speed(0.5f, half_speed_samples);
        render_at_speed(2.0f, double_speed_samples);

        // Test that different speeds produce different audio
        // (This is a basic test - in practice, the audio should sound different)
        REQUIRE(normal_speed_samples.size() == BUFFER_SIZE * NUM_FRAMES);
        REQUIRE(half_speed_samples.size() == BUFFER_SIZE * NUM_FRAMES);
        REQUIRE(double_speed_samples.size() == BUFFER_SIZE * NUM_FRAMES);

        // Test that we have audio content at all speeds
        {
            float max_normal = 0.0f, max_half = 0.0f, max_double = 0.0f;
            for (float sample : normal_speed_samples) max_normal = std::max(max_normal, std::abs(sample));
            for (float sample : half_speed_samples) max_half = std::max(max_half, std::abs(sample));
            for (float sample : double_speed_samples) max_double = std::max(max_double, std::abs(sample));
            
            REQUIRE(max_normal > 0.0f);
            REQUIRE(max_half > 0.0f);
            REQUIRE(max_double > 0.0f);
        }
    }
}

TEST_CASE("AudioFileGeneratorRenderStage - Content Accuracy Test", "[audio_file_generator_render_stage][gl_test][content]") {
    SDLWindow window(WIDTH, HEIGHT);
    GLContext context;

    constexpr int BUFFER_SIZE = WIDTH;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_CHANNELS = HEIGHT;
    constexpr float TEST_GAIN = 1.0f; // Use full gain for accurate comparison
    constexpr int NUM_FRAMES = SAMPLE_RATE / BUFFER_SIZE * 2; // 2 seconds

    const std::string test_file_path = "media/test.wav";

    SECTION("Content Comparison at Normal Speed") {
        // Load original audio data
        std::vector<std::vector<float>> original_data;
        try {
            original_data = load_original_audio_data(test_file_path);
        } catch (const std::exception& e) {
            FAIL("Failed to load original audio data: " << e.what());
        }

        REQUIRE(original_data.size() >= NUM_CHANNELS);
        REQUIRE(!original_data[0].empty());

        // Create file generator render stage
        AudioFileGeneratorRenderStage file_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, test_file_path);
        AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

        REQUIRE(file_generator.connect_render_stage(&final_render_stage));
        
        auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
        global_time_param->set_value(0);
        global_time_param->initialize();

        // Remove the envelope for clean comparison
        auto attack_param = file_generator.find_parameter("attack_time");
        attack_param->set_value(0.0f);
        auto decay_param = file_generator.find_parameter("decay_time");
        decay_param->set_value(0.0f);
        auto sustain_param = file_generator.find_parameter("sustain_level");
        sustain_param->set_value(1.0f);
        auto release_param = file_generator.find_parameter("release_time");
        release_param->set_value(0.0f);
        
        REQUIRE(file_generator.initialize());
        REQUIRE(final_render_stage.initialize());

        context.prepare_draw();
        REQUIRE(file_generator.bind());
        REQUIRE(final_render_stage.bind());

        // Play at normal speed
        file_generator.play_note(MIDDLE_C, TEST_GAIN);

        // Render frames and collect samples
        std::vector<float> rendered_samples;
        rendered_samples.reserve(BUFFER_SIZE * NUM_FRAMES);

        for (int frame = 0; frame < NUM_FRAMES; frame++) {
            global_time_param->set_value(frame);
            global_time_param->render();

            file_generator.render(frame);
            final_render_stage.render(frame);
            
            auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
            REQUIRE(output_param != nullptr);

            const float* output_data = static_cast<const float*>(output_param->get_value());
            REQUIRE(output_data != nullptr);

            for (int i = 0; i < BUFFER_SIZE; i++) {
                rendered_samples.push_back(output_data[i * NUM_CHANNELS]);
            }
        }

        REQUIRE(rendered_samples.size() == BUFFER_SIZE * NUM_FRAMES);

        // Compare with original data
        const size_t comparison_length = std::min(rendered_samples.size(), original_data[0].size());
        
        if (comparison_length > 0) {
            // Extract the corresponding portion of original data
            std::vector<float> original_portion(original_data[0].begin(), 
                                              original_data[0].begin() + comparison_length);
            
            // Calculate correlation
            float correlation = calculate_correlation(rendered_samples, original_portion);
            
            // Calculate RMS error
            float rms_error = calculate_rms_error(rendered_samples, original_portion);
            
            // Test correlation (should be high for accurate playback)
            REQUIRE(correlation > 0.7f); // High correlation indicates similar content
            
            // Test RMS error (should be low for accurate playback)
            REQUIRE(rms_error < 0.5f); // Low error indicates accurate reproduction
            
            // Test that we have non-zero content
            float max_rendered = 0.0f, max_original = 0.0f;
            for (float sample : rendered_samples) max_rendered = std::max(max_rendered, std::abs(sample));
            for (float sample : original_portion) max_original = std::max(max_original, std::abs(sample));
            
            REQUIRE(max_rendered > 0.0f);
            REQUIRE(max_original > 0.0f);
        }

        final_render_stage.unbind();
        file_generator.unbind();
        delete global_time_param;
    }

    SECTION("Content Comparison at Different Speeds") {
        // Load original audio data
        std::vector<std::vector<float>> original_data;
        try {
            original_data = load_original_audio_data(test_file_path);
        } catch (const std::exception& e) {
            FAIL("Failed to load original audio data: " << e.what());
        }

        REQUIRE(original_data.size() >= NUM_CHANNELS);
        REQUIRE(!original_data[0].empty());

        // Test different speeds
        std::vector<float> speeds = {0.5f, 1.0f, 2.0f};
        
        for (float speed : speeds) {
            // Create file generator render stage
            AudioFileGeneratorRenderStage file_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, test_file_path);
            AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

            REQUIRE(file_generator.connect_render_stage(&final_render_stage));
            
            auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
            global_time_param->set_value(0);
            global_time_param->initialize();

            // Remove the envelope
            auto attack_param = file_generator.find_parameter("attack_time");
            attack_param->set_value(0.0f);
            auto decay_param = file_generator.find_parameter("decay_time");
            decay_param->set_value(0.0f);
            auto sustain_param = file_generator.find_parameter("sustain_level");
            sustain_param->set_value(1.0f);
            auto release_param = file_generator.find_parameter("release_time");
            release_param->set_value(0.0f);
            
            REQUIRE(file_generator.initialize());
            REQUIRE(final_render_stage.initialize());

            context.prepare_draw();
            REQUIRE(file_generator.bind());
            REQUIRE(final_render_stage.bind());

            // Play at specified speed
            file_generator.play_note(MIDDLE_C * speed, TEST_GAIN);

            // Render frames and collect samples
            std::vector<float> rendered_samples;
            rendered_samples.reserve(BUFFER_SIZE * NUM_FRAMES);

            for (int frame = 0; frame < NUM_FRAMES; frame++) {
                global_time_param->set_value(frame);
                global_time_param->render();

                file_generator.render(frame);
                final_render_stage.render(frame);
                
                auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
                REQUIRE(output_param != nullptr);

                const float* output_data = static_cast<const float*>(output_param->get_value());
                REQUIRE(output_data != nullptr);

                for (int i = 0; i < BUFFER_SIZE; i++) {
                    rendered_samples.push_back(output_data[i * NUM_CHANNELS]);
                }
            }

            REQUIRE(rendered_samples.size() == BUFFER_SIZE * NUM_FRAMES);

            // Test that we have non-zero content at all speeds
            float max_amplitude = 0.0f;
            for (float sample : rendered_samples) {
                max_amplitude = std::max(max_amplitude, std::abs(sample));
            }
            REQUIRE(max_amplitude > 0.0f);

            final_render_stage.unbind();
            file_generator.unbind();
            delete global_time_param;
        }
    }
}

TEST_CASE("AudioGeneratorRenderStage - Direct Audio Output Test", "[audio_generator_render_stage][gl_test][audio_output]") {
    SDLWindow window(WIDTH, HEIGHT);
    GLContext context;

    constexpr int BUFFER_SIZE = WIDTH;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_CHANNELS = HEIGHT;
    constexpr float TEST_FREQUENCY = 450.0f; // A4 note
    constexpr float TEST_GAIN = 0.3f; // Lower gain to prevent clipping

    // Create sine generator render stage
    AudioGeneratorRenderStage sine_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 
                                            "build/shaders/multinote_sine_generator_render_stage.glsl");

    // Create final render stage for interpolation
    AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Connect the generator to the final render stage
    REQUIRE(sine_generator.connect_render_stage(&final_render_stage));

    // Add global_time parameter as a buffer parameter
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();

    // Remove the envelope for clean sine wave
    auto attack_param = sine_generator.find_parameter("attack_time");
    attack_param->set_value(0.0f);
    auto decay_param = sine_generator.find_parameter("decay_time");
    decay_param->set_value(0.0f);
    auto sustain_param = sine_generator.find_parameter("sustain_level");
    sustain_param->set_value(1.0f);
    auto release_param = sine_generator.find_parameter("release_time");
    release_param->set_value(0.0f);
    
    // Initialize the render stages
    REQUIRE(sine_generator.initialize());
    REQUIRE(final_render_stage.initialize());

    context.prepare_draw();
    // Bind the render stages
    REQUIRE(sine_generator.bind());
    REQUIRE(final_render_stage.bind());

    AudioPlayerOutput audio_output(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
    REQUIRE(audio_output.open());

    SECTION("Combined real-time and pre-recorded audio output") {
        // Run audio for a few seconds to test
        std::cout << "Playing A4 note (440 Hz) for 5 seconds with recording..." << std::endl;
        
        // Render and push audio data for 5 seconds
        constexpr int NUM_FRAMES = 5 * SAMPLE_RATE / BUFFER_SIZE; // 5 seconds
        
        // Vector to store recorded audio data
        std::vector<float> recorded_audio;
        recorded_audio.reserve(NUM_FRAMES * BUFFER_SIZE * NUM_CHANNELS);

        // Create audio output directly
        REQUIRE(audio_output.start());

        // Play a note
        sine_generator.play_note(TEST_FREQUENCY, TEST_GAIN);
        
        // Single loop: render once, save to recording, and play in real-time
        for (int frame = 0; frame < NUM_FRAMES; frame++) {
            global_time_param->set_value(frame);
            global_time_param->render();

            // Render the generator stage
            sine_generator.render(frame);
            
            // Render the final stage for interpolation
            final_render_stage.render(frame);
            
            // Get the final output audio data from the final render stage
            auto final_output_param = final_render_stage.find_parameter("final_output_audio_texture");
            REQUIRE(final_output_param != nullptr);
            
            const float* final_output_data = static_cast<const float*>(final_output_param->get_value());
            REQUIRE(final_output_data != nullptr);

            // Store the audio data in our vector for later playback
            for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; i++) {
                recorded_audio.push_back(final_output_data[i]);
            }

            // Wait for audio output to be ready
            while (!audio_output.is_ready()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
            // Push the interpolated audio data to the output for real-time playback
            audio_output.push(final_output_data);
        }

        // Let it settle for a moment
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Clean up real-time audio
        audio_output.stop();
        
        // Now play back the recorded audio
        std::cout << "Playing back recorded audio..." << std::endl;
        
        // Create new audio output for playback
        REQUIRE(audio_output.start());

        // Play back the recorded audio in chunks
        for (size_t offset = 0; offset < recorded_audio.size(); offset += BUFFER_SIZE * NUM_CHANNELS) {
            // Wait for audio output to be ready
            while (!audio_output.is_ready()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
            // Push the chunk of recorded audio data
            audio_output.push(&recorded_audio[offset]);
        }
        
        // Let it settle for a moment
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Clean up playback audio
        audio_output.stop();
        
        std::cout << "Pre-recorded audio playback complete." << std::endl;
    }

    // Stop the note
    sine_generator.stop_note(TEST_FREQUENCY);
    std::cout << "Stopped note." << std::endl;
        

    audio_output.close();
    final_render_stage.unbind();
    sine_generator.unbind();
    delete global_time_param;
}

TEST_CASE("AudioFileGeneratorRenderStage - Direct Audio Output Test", "[audio_file_generator_render_stage][gl_test][audio_output]") {
    SDLWindow window(WIDTH, HEIGHT);
    GLContext context;

    constexpr int BUFFER_SIZE = WIDTH;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_CHANNELS = HEIGHT;
    constexpr float TEST_GAIN = 0.5f;
    constexpr int NUM_FRAMES = SAMPLE_RATE / BUFFER_SIZE * 3; // 3 seconds

    const std::string test_file_path = "media/test.wav";

    SECTION("File Generator Real-time Playback") {
        // Create file generator render stage
        AudioFileGeneratorRenderStage file_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, test_file_path);
        AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

        // Connect the generator to the final render stage
        REQUIRE(file_generator.connect_render_stage(&final_render_stage));
        
        // Add global_time parameter as a buffer parameter
        auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
        global_time_param->set_value(0);
        global_time_param->initialize();

        // Remove the envelope for clean playback
        auto attack_param = file_generator.find_parameter("attack_time");
        attack_param->set_value(0.0f);
        auto decay_param = file_generator.find_parameter("decay_time");
        decay_param->set_value(0.0f);
        auto sustain_param = file_generator.find_parameter("sustain_level");
        sustain_param->set_value(1.0f);
        auto release_param = file_generator.find_parameter("release_time");
        release_param->set_value(0.0f);
        
        // Initialize the render stages
        REQUIRE(file_generator.initialize());
        REQUIRE(final_render_stage.initialize());

        context.prepare_draw();
        REQUIRE(file_generator.bind());
        REQUIRE(final_render_stage.bind());

        AudioPlayerOutput audio_output(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
        REQUIRE(audio_output.open());

        SECTION("Normal Speed Playback") {
            std::cout << "Playing test.wav at normal speed for 3 seconds..." << std::endl;
            
            // Create audio output
            REQUIRE(audio_output.start());

            // Play at normal speed
            file_generator.play_note(MIDDLE_C, TEST_GAIN);
            
            // Render and play audio data
            for (int frame = 0; frame < NUM_FRAMES; frame++) {
                global_time_param->set_value(frame);
                global_time_param->render();

                file_generator.render(frame);
                final_render_stage.render(frame);
                
                auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
                REQUIRE(output_param != nullptr);
                
                const float* output_data = static_cast<const float*>(output_param->get_value());
                REQUIRE(output_data != nullptr);

                // Wait for audio output to be ready
                while (!audio_output.is_ready()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                
                // Push the audio data to the output for real-time playback
                audio_output.push(output_data);
            }

            // Let it settle for a moment
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Clean up
            audio_output.stop();
            std::cout << "Normal speed playback complete." << std::endl;
        }

        SECTION("Half Speed Playback") {
            std::cout << "Playing test.wav at half speed for 3 seconds..." << std::endl;
            
            // Create audio output
            REQUIRE(audio_output.start());

            // Play at half speed
            file_generator.play_note(MIDDLE_C * 0.5f, TEST_GAIN);
            
            // Render and play audio data
            for (int frame = 0; frame < NUM_FRAMES; frame++) {
                global_time_param->set_value(frame);
                global_time_param->render();

                file_generator.render(frame);
                final_render_stage.render(frame);
                
                auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
                REQUIRE(output_param != nullptr);
                
                const float* output_data = static_cast<const float*>(output_param->get_value());
                REQUIRE(output_data != nullptr);

                // Wait for audio output to be ready
                while (!audio_output.is_ready()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                
                // Push the audio data to the output for real-time playback
                audio_output.push(output_data);
            }

            // Let it settle for a moment
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Clean up
            audio_output.stop();
            std::cout << "Half speed playback complete." << std::endl;
        }

        SECTION("Double Speed Playback") {
            std::cout << "Playing test.wav at double speed for 3 seconds..." << std::endl;
            
            // Create audio output
            REQUIRE(audio_output.start());

            // Play at double speed
            file_generator.play_note(MIDDLE_C * 2.0f, TEST_GAIN);
            
            // Render and play audio data
            for (int frame = 0; frame < NUM_FRAMES; frame++) {
                global_time_param->set_value(frame);
                global_time_param->render();

                file_generator.render(frame);
                final_render_stage.render(frame);
                
                auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
                REQUIRE(output_param != nullptr);
                
                const float* output_data = static_cast<const float*>(output_param->get_value());
                REQUIRE(output_data != nullptr);

                // Wait for audio output to be ready
                while (!audio_output.is_ready()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                
                // Push the audio data to the output for real-time playback
                audio_output.push(output_data);
            }

            // Let it settle for a moment
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Clean up
            audio_output.stop();
            std::cout << "Double speed playback complete." << std::endl;
        }

        SECTION("Combined Real-time and Pre-recorded Playback") {
            std::cout << "Playing test.wav with recording and playback..." << std::endl;
            
            // Vector to store recorded audio data
            std::vector<float> recorded_audio;
            recorded_audio.reserve(NUM_FRAMES * BUFFER_SIZE * NUM_CHANNELS);

            // Create audio output
            REQUIRE(audio_output.start());

            // Play at normal speed
            file_generator.play_note(MIDDLE_C, TEST_GAIN);
            
            // Render, record, and play audio data simultaneously
            for (int frame = 0; frame < NUM_FRAMES; frame++) {
                global_time_param->set_value(frame);
                global_time_param->render();

                file_generator.render(frame);
                final_render_stage.render(frame);
                
                auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
                REQUIRE(output_param != nullptr);
                
                const float* output_data = static_cast<const float*>(output_param->get_value());
                REQUIRE(output_data != nullptr);

                // Store the audio data for later playback
                for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; i++) {
                    recorded_audio.push_back(output_data[i]);
                }

                // Wait for audio output to be ready
                while (!audio_output.is_ready()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                
                // Push the audio data to the output for real-time playback
                audio_output.push(output_data);
            }

            // Let it settle for a moment
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Clean up real-time audio
            audio_output.stop();
            
            // Now play back the recorded audio
            std::cout << "Playing back recorded audio..." << std::endl;
            
            // Create new audio output for playback
            REQUIRE(audio_output.start());

            // Play back the recorded audio in chunks
            for (size_t offset = 0; offset < recorded_audio.size(); offset += BUFFER_SIZE * NUM_CHANNELS) {
                // Wait for audio output to be ready
                while (!audio_output.is_ready()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                
                // Push the chunk of recorded audio data
                audio_output.push(&recorded_audio[offset]);
            }
            
            // Let it settle for a moment
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Clean up playback audio
            audio_output.stop();
            
            std::cout << "Pre-recorded audio playback complete." << std::endl;
        }

        // Stop the note
        file_generator.stop_note(MIDDLE_C);
        std::cout << "Stopped file playback." << std::endl;

        audio_output.close();
        final_render_stage.unbind();
        file_generator.unbind();
        delete global_time_param;
    }
}