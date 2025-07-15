#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"
#include "utilities/egl_compatibility.h"

#include "audio_core/audio_render_stage.h"
#include "audio_core/audio_render_graph.h"
#include "audio_core/audio_renderer.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_parameter/audio_texture2d_parameter.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"
#include "audio_output/audio_player_output.h"
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

TEST_CASE("AudioGeneratorRenderStage - Sine Wave Generation", "[audio_generator_render_stage][gl_test]") {
    SDLWindow window(WIDTH, HEIGHT);
    GLContext context;

    constexpr int BUFFER_SIZE = WIDTH;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_CHANNELS = HEIGHT;
    constexpr float TEST_FREQUENCY = 450.0f; // A4 note
    constexpr float TEST_GAIN = 0.3f;

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
    constexpr int NUM_FRAMES = SAMPLE_RATE / BUFFER_SIZE * 5; // 5 seconds
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
        // Left channel: indices 0 to BUFFER_SIZE-1
        // Right channel: indices BUFFER_SIZE to 2*BUFFER_SIZE-1
        for (int i = 0; i < BUFFER_SIZE; i++) {
            left_channel_samples.push_back(output_data[i * NUM_CHANNELS]);
            right_channel_samples.push_back(output_data[i * NUM_CHANNELS + 1]);
        }
    }

    // Analyze the waveform for exact sine wave characteristics
    REQUIRE(left_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);
    REQUIRE(right_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);

    SECTION("Exact sine wave validation") {
        // Test both channels
        std::vector<std::vector<float>*> channels = {&left_channel_samples, &right_channel_samples};
        std::vector<std::string> channel_names = {"Left", "Right"};
        
        for (size_t ch = 0; ch < channels.size(); ch++) {
            const auto& samples = *channels[ch];
            const std::string& name = channel_names[ch];
            
            INFO("Testing " << name << " channel");
            
            // Test 1: Check for exact frequency
            {
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

            // Test 2: Check for exact amplitude
            {
                float max_amplitude = 0.0f;
                for (float sample : samples) {
                    max_amplitude = std::max(max_amplitude, std::abs(sample));
                }

                // Should be exactly TEST_GAIN
                REQUIRE(max_amplitude == Catch::Approx(TEST_GAIN).margin(0.01f));
            }

            // Test 3: Check for glitches/discontinuities
            {
                // FIXME: Fiture out clipping i
                constexpr float MAX_SAMPLE_DIFF = 0.01f; // Very strict tolerance for exact sine wave
                
                for (size_t i = 1; i < samples.size(); ++i) {
                    float sample_diff = std::abs(samples[i] - samples[i-1]);
                    
                    // Check for sudden jumps that would indicate glitches
                    REQUIRE(sample_diff <= MAX_SAMPLE_DIFF);
                }
            }

            // Test 4: Check for zero DC offset
            {
                float sum = 0.0f;
                for (float sample : samples) {
                    sum += sample;
                }
                float dc_offset = sum / samples.size();
                
                // DC offset should be exactly zero for a perfect sine wave
                REQUIRE(std::abs(dc_offset) < 0.001f);
            }

            // Test 5: Check for exact sine wave shape
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

            // Test 6: Check for NaN or infinite values
            {
                for (float sample : samples) {
                    REQUIRE(!std::isnan(sample));
                    REQUIRE(!std::isinf(sample));
                }
            }

            // Test 7: Check for clipping
            {
                for (float sample : samples) {
                    // Samples should not exceed the gain value
                    REQUIRE(std::abs(sample) <= TEST_GAIN);
                }
            }

            // Test 8: Check phase continuity
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

    SECTION("Channel correlation test") {
        // Both channels should be identical for a mono sine wave
        REQUIRE(left_channel_samples.size() == right_channel_samples.size());
        
        // Check that both channels are identical
        for (size_t i = 0; i < left_channel_samples.size(); ++i) {
            REQUIRE(left_channel_samples[i] == Catch::Approx(right_channel_samples[i]).margin(0.001f));
        }
    }

    final_render_stage.unbind();
    sine_generator.unbind();
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