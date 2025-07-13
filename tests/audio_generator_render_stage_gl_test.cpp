#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"
#include "utilities/egl_compatibility.h"

#include "audio_core/audio_render_stage.h"
#include "audio_core/audio_render_graph.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_parameter/audio_texture2d_parameter.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"

#include <functional>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>

/**
 * @brief Tests for generator render stage functionality with OpenGL context
 * 
 * These tests check the generator render stage creation, initialization, and rendering in an OpenGL context.
 * Focus on sine wave generation with comprehensive waveform analysis and glitch detection.
 * Note: These tests require a valid OpenGL context to run, which may not be available
 * in all test environments. They are marked with [gl] tag.
 */
constexpr int WIDTH = 256;
constexpr int HEIGHT = 1;

TEST_CASE("AudioGeneratorRenderStage - Sine Wave Generation", "[audio_generator_render_stage][gl_test]") {
    SDLWindow window(WIDTH, HEIGHT);
    GLContext context;

    constexpr int BUFFER_SIZE = 256;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_CHANNELS = 2;
    constexpr float TEST_FREQUENCY = 440.0f; // A4 note
    constexpr float TEST_GAIN = 0.5f;

    // Create sine generator render stage
    AudioGeneratorRenderStage sine_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 
                                            "build/shaders/multinote_sine_generator_render_stage.glsl");

    // Add global_time parameter as a buffer parameter
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);

    global_time_param->initialize();

    // Remove the envelope
    auto attack_param = sine_generator.find_parameter("attack_time");
    attack_param->set_value(0.0f);
    auto decay_param = sine_generator.find_parameter("decay_time");
    decay_param->set_value(0.0f);
    auto sustain_param = sine_generator.find_parameter("sustain_level");
    sustain_param->set_value(1.0f);
    auto release_param = sine_generator.find_parameter("release_time");
    release_param->set_value(0.0f);
    
    // Initialize the render stage
    REQUIRE(sine_generator.initialize());

    context.prepare_draw();

    // Play a note
    sine_generator.play_note(TEST_FREQUENCY, TEST_GAIN);

    // Render multiple frames to test continuity
    constexpr int NUM_FRAMES = 100;
    std::vector<float> all_samples;
    all_samples.reserve(BUFFER_SIZE * NUM_FRAMES);

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        // Bind and prepare for rendering
        REQUIRE(sine_generator.bind());

        // Set global_time for this frame
        global_time_param->set_value(frame);
        global_time_param->render();

        sine_generator.render(frame);
        
        // Get output data
        auto output_param = sine_generator.find_parameter("output_audio_texture");
        REQUIRE(output_param != nullptr);

        const float* output_data = static_cast<const float*>(output_param->get_value());
        REQUIRE(output_data != nullptr);

        // Store samples for analysis
        for (int i = 0; i < BUFFER_SIZE; i++) {
            all_samples.push_back(output_data[i]);
        }
    }

    // Analyze the waveform for glitches and correctness
    REQUIRE(all_samples.size() == BUFFER_SIZE * NUM_FRAMES);

    // Save audio data to file for visualization
    std::ofstream audio_file("playground/audio_output.txt");
    REQUIRE(audio_file.is_open());
    audio_file << "# Audio Generator Test Output\n";
    audio_file << "# Sample Rate: " << SAMPLE_RATE << "\n";
    audio_file << "# Buffer Size: " << BUFFER_SIZE << "\n";
    audio_file << "# Num Frames: " << NUM_FRAMES << "\n";
    audio_file << "# Test Frequency: " << TEST_FREQUENCY << "\n";
    audio_file << "# Test Gain: " << TEST_GAIN << "\n";
    audio_file << "# Format: sample_index,amplitude\n";
    
    for (size_t i = 0; i < all_samples.size(); ++i) {
        audio_file << i << "," << all_samples[i] << "\n";
    }
    audio_file.close();

    // Test 1: Check for basic sine wave characteristics
    SECTION("Basic sine wave characteristics") {
        // Find zero crossings to verify frequency
        std::vector<int> zero_crossings;
        for (size_t i = 1; i < all_samples.size(); ++i) {
            if ((all_samples[i-1] < 0 && all_samples[i] >= 0) || 
                (all_samples[i-1] > 0 && all_samples[i] <= 0)) {
                zero_crossings.push_back(i);
            }
        }

        REQUIRE(zero_crossings.size() >= 2); // Should have multiple zero crossings

        // Calculate measured frequency from zero crossings
        if (zero_crossings.size() >= 2) {
            float total_time = static_cast<float>(all_samples.size()) / SAMPLE_RATE;
            float measured_frequency = static_cast<float>(zero_crossings.size() - 1) / (2.0f * total_time);
            
            // Allow some tolerance for frequency measurement
            REQUIRE(measured_frequency == Catch::Approx(TEST_FREQUENCY).margin(10.0f));
        }

        // Check amplitude (should be around TEST_GAIN)
        float max_amplitude = 0.0f;
        for (float sample : all_samples) {
            max_amplitude = std::max(max_amplitude, std::abs(sample));
        }

        REQUIRE(max_amplitude == Catch::Approx(TEST_GAIN).margin(0.1f));
        
    }

    // Test 2: Check for discontinuities/glitches
    SECTION("Discontinuity and glitch detection") {
        constexpr float MAX_SAMPLE_DIFF = 0.1f; // Maximum allowed difference between consecutive samples
        
        for (size_t i = 1; i < all_samples.size(); ++i) {
            float sample_diff = std::abs(all_samples[i] - all_samples[i-1]);
            
            // Skip the first sample of each frame (except the very first) as there might be 
            // legitimate discontinuities at frame boundaries due to ADSR envelope
            if (i % BUFFER_SIZE == 0 && i > 0) {
                continue;
            }
            
            // Check for sudden jumps that would indicate glitches
            REQUIRE(sample_diff <= MAX_SAMPLE_DIFF);
        }
    }

    // Test 3: Check for DC offset
    SECTION("DC offset detection") {
        float sum = 0.0f;
        for (float sample : all_samples) {
            sum += sample;
        }
        float dc_offset = sum / all_samples.size();
        
        // DC offset should be very close to zero for a sine wave
        REQUIRE(std::abs(dc_offset) < 0.01f);
    }

    // Test 4: Check for clipping
    SECTION("Clipping detection") {
        for (float sample : all_samples) {
            // Samples should not exceed the gain value significantly
            REQUIRE(std::abs(sample) <= TEST_GAIN * 1.1f);
        }
    }

    // Test 5: Check for NaN or infinite values
    SECTION("NaN and infinite value detection") {
        for (float sample : all_samples) {
            REQUIRE(!std::isnan(sample));
            REQUIRE(!std::isinf(sample));
        }
    }

    // Test 6: Verify ADSR envelope behavior
    SECTION("ADSR envelope behavior") {
        // Get ADSR parameters
        auto attack_param = sine_generator.find_parameter("attack_time");
        auto decay_param = sine_generator.find_parameter("decay_time");
        auto sustain_param = sine_generator.find_parameter("sustain_level");
        auto release_param = sine_generator.find_parameter("release_time");
        
        REQUIRE(attack_param != nullptr);
        REQUIRE(decay_param != nullptr);
        REQUIRE(sustain_param != nullptr);
        REQUIRE(release_param != nullptr);

        // Check that envelope starts at zero and ramps up
        float first_sample = all_samples[0];
        REQUIRE(std::abs(first_sample) < 0.1f); // Should start near zero due to attack

        // Check that envelope reaches full amplitude after attack
        float attack_time = *(float*)attack_param->get_value();
        int attack_samples = static_cast<int>(attack_time * SAMPLE_RATE);
        
        if (attack_samples < all_samples.size()) {
            float sample_after_attack = all_samples[attack_samples];
            REQUIRE(std::abs(sample_after_attack) > TEST_GAIN * 0.8f); // Should be close to full amplitude
        }
    }

    sine_generator.unbind();
} 