#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"
#include "framework/test_main.h"

#include "audio_core/audio_render_stage.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_render_stage/audio_file_generator_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"
#include "audio_output/audio_player_output.h"
#include "audio_output/audio_wav.h"
#include "framework/csv_test_output.h"

#include <X11/X.h>
#include <iomanip>
#include <sstream>
#include <functional>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>
#include <numeric>
#include <utility>

/**
 * @brief Tests for generator render stage functionality with OpenGL context
 * 
 * These tests check the generator render stage creation, initialization, and rendering in an OpenGL context.
 * Focus on sine wave generation with comprehensive waveform analysis and glitch detection.
 * Note: These tests require a valid OpenGL context to run, which may not be available
 * in all test environments. They are marked with [gl] tag.
 */

// Test parameter structure to hold buffer size and channel count combinations
struct TestParams {
    int buffer_size;
    int num_channels;
    const char* name;
};

// Define 3 different test parameter combinations
using TestParam1 = std::integral_constant<int, 0>; // 256 buffer, 2 channels
using TestParam2 = std::integral_constant<int, 1>; // 512 buffer, 2 channels  
using TestParam3 = std::integral_constant<int, 2>; // 1024 buffer, 2 channels

// Parameter lookup function
constexpr TestParams get_test_params(int index) {
    constexpr TestParams params[] = {
        {256, 2, "256_buffer_2_channels"},
        {512, 2, "512_buffer_2_channels"},
        {1024, 2, "1024_buffer_2_channels"}
    };
    return params[index];
}

// Note: MIDDLE_C is already defined as a macro in audio_generator_render_stage.h

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

// Helper function to calculate correlation for a specific offset without creating new vectors
float calculate_correlation_at_offset(const std::vector<float>& a, const std::vector<float>& b, int offset) {
    size_t start_a, start_b, len;
    
    if (offset > 0) {
        // b is delayed: start a at offset
        start_a = offset;
        start_b = 0;
        len = std::min(a.size() - start_a, b.size() - start_b);
    } else if (offset < 0) {
        // b is ahead: start b at -offset
        start_a = 0;
        start_b = -offset;
        len = std::min(a.size() - start_a, b.size() - start_b);
    } else {
        // No offset
        start_a = 0;
        start_b = 0;
        len = std::min(a.size(), b.size());
    }
    
    if (len == 0) return 0.0f;
    
    float sum_a = 0.0f, sum_b = 0.0f, sum_ab = 0.0f, sum_a2 = 0.0f, sum_b2 = 0.0f;
    
    for (size_t i = 0; i < len; ++i) {
        float val_a = a[start_a + i];
        float val_b = b[start_b + i];
        sum_a += val_a;
        sum_b += val_b;
        sum_ab += val_a * val_b;
        sum_a2 += val_a * val_a;
        sum_b2 += val_b * val_b;
    }

    float n = static_cast<float>(len);
    float numerator = n * sum_ab - sum_a * sum_b;
    float denominator = std::sqrt((n * sum_a2 - sum_a * sum_a) * (n * sum_b2 - sum_b * sum_b));
    
    return denominator != 0.0f ? numerator / denominator : 0.0f;
}

// Helper function to find best correlation with time offset (cross-correlation)
// Returns: (best_correlation, best_offset)
// Positive offset means output is delayed relative to original
// Negative offset means output is ahead of original
std::pair<float, int> find_best_correlation_with_offset(const std::vector<float>& original, 
                                                          const std::vector<float>& output,
                                                          int max_offset_samples = 5000) {
    if (original.empty() || output.empty()) {
        return {0.0f, 0};
    }
    
    // Limit search range to reasonable bounds
    const size_t min_size = std::min(original.size(), output.size());
    const int max_offset = std::min(max_offset_samples, static_cast<int>(min_size / 2));
    
    float best_correlation = -1.0f;
    int best_offset = 0;
    
    // Try different offsets (use step size for large ranges to speed up)
    int step = (max_offset > 1000) ? 4 : 1; // Coarse search for large ranges
    
    // First pass: coarse search
    for (int offset = -max_offset; offset <= max_offset; offset += step) {
        float correlation = calculate_correlation_at_offset(original, output, offset);
        
        if (correlation > best_correlation) {
            best_correlation = correlation;
            best_offset = offset;
        }
    }
    
    // Second pass: fine search around best offset (if step > 1)
    if (step > 1 && best_offset != 0) {
        int fine_start = std::max(-max_offset, best_offset - step);
        int fine_end = std::min(max_offset, best_offset + step);
        
        for (int offset = fine_start; offset <= fine_end; offset++) {
            float correlation = calculate_correlation_at_offset(original, output, offset);
            
            if (correlation > best_correlation) {
                best_correlation = correlation;
                best_offset = offset;
            }
        }
    }
    
    return {best_correlation, best_offset};
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

// Helper function to resample audio data to match a speed ratio
// speed_ratio: 0.5 = half speed (stretched), 2.0 = double speed (compressed)
// Uses linear interpolation for resampling
std::vector<float> resample_audio(const std::vector<float>& original, float speed_ratio) {
    if (original.empty() || speed_ratio <= 0.0f) {
        return {};
    }
    
    // Calculate output size based on speed ratio
    // At 0.5x speed: output is 2x longer (stretched)
    // At 2.0x speed: output is 0.5x shorter (compressed)
    size_t output_size = static_cast<size_t>(static_cast<float>(original.size()) / speed_ratio);
    
    std::vector<float> resampled(output_size);
    
    for (size_t i = 0; i < output_size; ++i) {
        // Calculate source position in original
        float source_pos = static_cast<float>(i) * speed_ratio;
        size_t source_idx = static_cast<size_t>(source_pos);
        float fraction = source_pos - static_cast<float>(source_idx);
        
        if (source_idx >= original.size() - 1) {
            // Beyond end of original, use last sample
            resampled[i] = original.back();
        } else {
            // Linear interpolation
            resampled[i] = original[source_idx] * (1.0f - fraction) + original[source_idx + 1] * fraction;
        }
    }
    
    return resampled;
}

TEMPLATE_TEST_CASE("AudioGeneratorRenderStage - Sine Wave Generation", "[audio_generator_render_stage][gl_test][audio_output][csv_output][template]",
                   TestParam1, TestParam2, TestParam3) {
    
    // Get test parameters for this template instantiation
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;

    // Initialize window and OpenGL context with appropriate dimensions
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;
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
    sine_generator.play_note({TEST_FREQUENCY, TEST_GAIN});

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

    // Setup audio output (only if enabled)
    AudioPlayerOutput* audio_output = nullptr;
    if (is_audio_output_enabled()) {
        audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
        REQUIRE(audio_output->open());
        REQUIRE(audio_output->start());
        
        // Play back the rendered audio
        for (int frame = 0; frame < NUM_FRAMES; frame++) {
            global_time_param->set_value(frame);
            global_time_param->render();
            
            sine_generator.render(frame);
            final_render_stage.render(frame);
            
            auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
            REQUIRE(output_param != nullptr);
            
            const float* output_data = static_cast<const float*>(output_param->get_value());
            REQUIRE(output_data != nullptr);
            
            while (!audio_output->is_ready()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            audio_output->push(output_data);
        }
        
        // Wait for audio to finish
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        audio_output->stop();
        audio_output->close();
        delete audio_output;
    }

    // CSV output (only if enabled)
    if (is_csv_output_enabled()) {
        SECTION("Write output audio to CSV") {
            std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
            output_samples_per_channel[0] = left_channel_samples;
            output_samples_per_channel[1] = right_channel_samples;
            
            std::string csv_output_dir = "build/tests/csv_output";
            system(("mkdir -p " + csv_output_dir).c_str());
            
            std::ostringstream filename_stream;
            filename_stream << csv_output_dir << "/sine_wave_generation_buffer_" << BUFFER_SIZE 
                          << "_channels_" << NUM_CHANNELS << "_freq_" << TEST_FREQUENCY << ".csv";
            std::string filename = filename_stream.str();
            
            CSVTestOutput csv_writer(filename, SAMPLE_RATE);
            REQUIRE(csv_writer.is_open());
            
            csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
            csv_writer.close();
            
            std::cout << "Wrote sine wave output to " << filename << " (" 
                      << left_channel_samples.size() << " samples, " << NUM_CHANNELS << " channels)" << std::endl;
        }
    }

    final_render_stage.unbind();
    sine_generator.unbind();
    delete global_time_param;
}

TEMPLATE_TEST_CASE("AudioGeneratorRenderStage - Direct Audio Output Test", "[audio_generator_render_stage][gl_test][audio_output][csv_output][template]",
                   TestParam1, TestParam2, TestParam3) {
    
    // Get test parameters for this template instantiation
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;

    // Initialize window and OpenGL context with appropriate dimensions
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;
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

    // Setup audio output (only if enabled)
    AudioPlayerOutput* audio_output = nullptr;
    if (is_audio_output_enabled()) {
        audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
        REQUIRE(audio_output->open());
    }

    SECTION("Combined real-time and pre-recorded audio output") {
        // Run audio for a few seconds to test
        std::cout << "Playing A4 note (440 Hz) for 5 seconds with recording..." << std::endl;
        
        // Render and push audio data for 5 seconds
        constexpr int NUM_FRAMES = 5 * SAMPLE_RATE / BUFFER_SIZE; // 5 seconds
        
        // Vector to store recorded audio data
        std::vector<float> recorded_audio;
        recorded_audio.reserve(NUM_FRAMES * BUFFER_SIZE * NUM_CHANNELS);
        std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            output_samples_per_channel[ch].reserve(NUM_FRAMES * BUFFER_SIZE);
        }

        // Create audio output directly (only if enabled)
        if (audio_output) {
            REQUIRE(audio_output->start());
        }

        // Play a note
        sine_generator.play_note({TEST_FREQUENCY, TEST_GAIN});
        
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

            // Also collect per-channel samples for CSV output
            for (int i = 0; i < BUFFER_SIZE; i++) {
                for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                    output_samples_per_channel[ch].push_back(final_output_data[i * NUM_CHANNELS + ch]);
                }
            }

            // Wait for audio output to be ready
            //while (!audio_output.is_ready()) {
            //    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            //}
            
            //// Push the interpolated audio data to the output for real-time playback
            //audio_output.push(final_output_data);
        }

        // Let it settle for a moment
        //std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Clean up real-time audio (only if enabled)
        if (audio_output) {
            audio_output->stop();
        }
        
        // Now play back the recorded audio
        std::cout << "Playing back recorded audio..." << std::endl;
        
        // Create new audio output for playback (only if enabled)
        if (audio_output) {
            REQUIRE(audio_output->start());
        }

        // Play back the recorded audio in chunks
        for (size_t offset = 0; offset < recorded_audio.size(); offset += BUFFER_SIZE * NUM_CHANNELS) {
            // Wait for audio output to be ready
            //while (!audio_output.is_ready()) {
            //    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            //}
            
            //// Push the chunk of recorded audio data
            //audio_output.push(&recorded_audio[offset]);
        }
        
        // Let it settle for a moment
        //std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Clean up playback audio (only if enabled)
        if (audio_output) {
            audio_output->stop();
        }
        
        std::cout << "Pre-recorded audio playback complete." << std::endl;

        // CSV output (only if enabled)
        if (is_csv_output_enabled()) {
            std::string csv_output_dir = "build/tests/csv_output";
            system(("mkdir -p " + csv_output_dir).c_str());
            
            std::ostringstream filename_stream;
            filename_stream << csv_output_dir << "/sine_generator_direct_audio_output_buffer_" 
                          << BUFFER_SIZE << "_channels_" << NUM_CHANNELS << "_freq_" << TEST_FREQUENCY << ".csv";
            std::string filename = filename_stream.str();
            
            CSVTestOutput csv_writer(filename, SAMPLE_RATE);
            REQUIRE(csv_writer.is_open());
            
            csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
            csv_writer.close();
            
            std::cout << "Wrote direct audio output to " << filename << " (" 
                      << output_samples_per_channel[0].size() << " samples, " << NUM_CHANNELS << " channels)" << std::endl;
        }
    }

    // Stop the note
    sine_generator.stop_note(TEST_FREQUENCY);
    std::cout << "Stopped note." << std::endl;
        
    // Cleanup audio output (only if enabled)
    if (audio_output) {
        audio_output->close();
        delete audio_output;
    }

    final_render_stage.unbind();
    sine_generator.unbind();
    delete global_time_param;
}

// FIXME: This test is not working as expected, fix using debug display
TEMPLATE_TEST_CASE("AudioFileGeneratorRenderStage - Direct Audio Output Test", "[audio_file_generator_render_stage][gl_test][audio_output][csv_output][template]",
                   TestParam1, TestParam2, TestParam3) {
    
    // Get test parameters for this template instantiation
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;

    // Initialize window and OpenGL context with appropriate dimensions
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;
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

        // Setup audio output (only if enabled)
        AudioPlayerOutput* audio_output = nullptr;
        if (is_audio_output_enabled()) {
            audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
            REQUIRE(audio_output->open());
        }

        SECTION("Normal Speed Playback") {
            std::cout << "Playing test.wav at normal speed for 3 seconds..." << std::endl;
            
            // Create audio output (only if enabled)
            if (audio_output) {
                REQUIRE(audio_output->start());
            }

            // Play at normal speed
            file_generator.play_note({MIDDLE_C, TEST_GAIN});
            
            // Collect samples for CSV output
            std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                output_samples_per_channel[ch].reserve(NUM_FRAMES * BUFFER_SIZE);
            }
            
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

                // Collect samples for CSV
                for (int i = 0; i < BUFFER_SIZE; i++) {
                    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                        output_samples_per_channel[ch].push_back(output_data[i * NUM_CHANNELS + ch]);
                    }
                }

                // Push to audio output if enabled
                if (audio_output) {
                    while (!audio_output->is_ready()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    audio_output->push(output_data);
                }
            }

            // Let it settle for a moment
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Clean up (only if enabled)
            if (audio_output) {
                audio_output->stop();
            }
            std::cout << "Normal speed playback complete." << std::endl;

            // CSV output (only if enabled)
            if (is_csv_output_enabled()) {
                std::string csv_output_dir = "build/tests/csv_output";
                system(("mkdir -p " + csv_output_dir).c_str());
                
                std::ostringstream filename_stream;
                filename_stream << csv_output_dir << "/file_generator_direct_audio_normal_speed_buffer_" 
                              << BUFFER_SIZE << "_channels_" << NUM_CHANNELS << ".csv";
                std::string filename = filename_stream.str();
                
                CSVTestOutput csv_writer(filename, SAMPLE_RATE);
                REQUIRE(csv_writer.is_open());
                
                csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
                csv_writer.close();
                
                std::cout << "Wrote direct audio output to " << filename << " (" 
                          << output_samples_per_channel[0].size() << " samples, " << NUM_CHANNELS << " channels)" << std::endl;
            }
        }

        SECTION("Half Speed Playback") {
            std::cout << "Playing test.wav at half speed for 3 seconds..." << std::endl;
            
            // Create audio output (only if enabled)
            if (audio_output) {
                REQUIRE(audio_output->start());
            }

            // Play at half speed
            file_generator.play_note({MIDDLE_C * 0.5f, TEST_GAIN});
            
            // Collect samples for CSV output
            std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                output_samples_per_channel[ch].reserve(NUM_FRAMES * BUFFER_SIZE);
            }
            
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

                // Collect samples for CSV
                for (int i = 0; i < BUFFER_SIZE; i++) {
                    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                        output_samples_per_channel[ch].push_back(output_data[i * NUM_CHANNELS + ch]);
                    }
                }

                // Push to audio output if enabled
                if (audio_output) {
                    while (!audio_output->is_ready()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    audio_output->push(output_data);
                }
            }

            // Let it settle for a moment
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Clean up (only if enabled)
            if (audio_output) {
                audio_output->stop();
            }
            std::cout << "Half speed playback complete." << std::endl;

            // CSV output (only if enabled)
            if (is_csv_output_enabled()) {
                std::string csv_output_dir = "build/tests/csv_output";
                system(("mkdir -p " + csv_output_dir).c_str());
                
                std::ostringstream filename_stream;
                filename_stream << csv_output_dir << "/file_generator_direct_audio_half_speed_buffer_" 
                              << BUFFER_SIZE << "_channels_" << NUM_CHANNELS << ".csv";
                std::string filename = filename_stream.str();
                
                CSVTestOutput csv_writer(filename, SAMPLE_RATE);
                REQUIRE(csv_writer.is_open());
                
                csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
                csv_writer.close();
                
                std::cout << "Wrote direct audio output to " << filename << " (" 
                          << output_samples_per_channel[0].size() << " samples, " << NUM_CHANNELS << " channels)" << std::endl;
            }
        }

        SECTION("Double Speed Playback") {
            std::cout << "Playing test.wav at double speed for 3 seconds..." << std::endl;
            
            // Create audio output (only if enabled)
            if (audio_output) {
                REQUIRE(audio_output->start());
            }

            // Play at double speed
            file_generator.play_note({MIDDLE_C * 2.0f, TEST_GAIN});
            
            // Collect samples for CSV output
            std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                output_samples_per_channel[ch].reserve(NUM_FRAMES * BUFFER_SIZE);
            }
            
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

                // Collect samples for CSV
                for (int i = 0; i < BUFFER_SIZE; i++) {
                    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                        output_samples_per_channel[ch].push_back(output_data[i * NUM_CHANNELS + ch]);
                    }
                }

                // Push to audio output if enabled
                if (audio_output) {
                    while (!audio_output->is_ready()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    audio_output->push(output_data);
                }
            }

            // Let it settle for a moment
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Clean up (only if enabled)
            if (audio_output) {
                audio_output->stop();
            }
            std::cout << "Double speed playback complete." << std::endl;

            // CSV output (only if enabled)
            if (is_csv_output_enabled()) {
                std::string csv_output_dir = "build/tests/csv_output";
                system(("mkdir -p " + csv_output_dir).c_str());
                
                std::ostringstream filename_stream;
                filename_stream << csv_output_dir << "/file_generator_direct_audio_double_speed_buffer_" 
                              << BUFFER_SIZE << "_channels_" << NUM_CHANNELS << ".csv";
                std::string filename = filename_stream.str();
                
                CSVTestOutput csv_writer(filename, SAMPLE_RATE);
                REQUIRE(csv_writer.is_open());
                
                csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
                csv_writer.close();
                
                std::cout << "Wrote direct audio output to " << filename << " (" 
                          << output_samples_per_channel[0].size() << " samples, " << NUM_CHANNELS << " channels)" << std::endl;
            }
        }

        SECTION("Combined Real-time and Pre-recorded Playback") {
            std::cout << "Playing test.wav with recording and playback..." << std::endl;
            
            // Vector to store recorded audio data
            std::vector<float> recorded_audio;
            recorded_audio.reserve(NUM_FRAMES * BUFFER_SIZE * NUM_CHANNELS);
            std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                output_samples_per_channel[ch].reserve(NUM_FRAMES * BUFFER_SIZE);
            }

            // Create audio output (only if enabled)
            if (audio_output) {
                REQUIRE(audio_output->start());
            }

            // Play at normal speed
            file_generator.play_note({MIDDLE_C, TEST_GAIN});
            
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

                // Also collect per-channel samples for CSV output
                for (int i = 0; i < BUFFER_SIZE; i++) {
                    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                        output_samples_per_channel[ch].push_back(output_data[i * NUM_CHANNELS + ch]);
                    }
                }

                // Push to audio output if enabled
                if (audio_output) {
                    while (!audio_output->is_ready()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    audio_output->push(output_data);
                }
            }

            // Let it settle for a moment
            //std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Clean up real-time audio (only if enabled)
            if (audio_output) {
                audio_output->stop();
            }
            
            // Now play back the recorded audio
            std::cout << "Playing back recorded audio..." << std::endl;
            
            // Create new audio output for playback (only if enabled)
            if (audio_output) {
                REQUIRE(audio_output->start());
            }

            // Play back the recorded audio in chunks
            for (size_t offset = 0; offset < recorded_audio.size(); offset += BUFFER_SIZE * NUM_CHANNELS) {
                // Push to audio output if enabled
                if (audio_output) {
                    while (!audio_output->is_ready()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    audio_output->push(&recorded_audio[offset]);
                }
            }
            
            // Let it settle for a moment
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Clean up playback audio (only if enabled)
            if (audio_output) {
                audio_output->stop();
            }
            
            std::cout << "Pre-recorded audio playback complete." << std::endl;

            // CSV output (only if enabled)
            if (is_csv_output_enabled()) {
                std::string csv_output_dir = "build/tests/csv_output";
                system(("mkdir -p " + csv_output_dir).c_str());
                
                std::ostringstream filename_stream;
                filename_stream << csv_output_dir << "/file_generator_direct_audio_combined_playback_buffer_" 
                              << BUFFER_SIZE << "_channels_" << NUM_CHANNELS << ".csv";
                std::string filename = filename_stream.str();
                
                CSVTestOutput csv_writer(filename, SAMPLE_RATE);
                REQUIRE(csv_writer.is_open());
                
                csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
                csv_writer.close();
                
                std::cout << "Wrote direct audio output to " << filename << " (" 
                          << output_samples_per_channel[0].size() << " samples, " << NUM_CHANNELS << " channels)" << std::endl;
            }
        }

        // Stop the note
        file_generator.stop_note(MIDDLE_C);
        std::cout << "Stopped file playback." << std::endl;

        // Cleanup audio output (only if enabled)
        if (audio_output) {
            audio_output->close();
            delete audio_output;
        }
        final_render_stage.unbind();
        file_generator.unbind();
        delete global_time_param;
    }
}

TEMPLATE_TEST_CASE("AudioFileGeneratorRenderStage - WAV File Comparison Test", "[audio_file_generator_render_stage][gl_test][audio_output][csv_output][template]",
                   TestParam1, TestParam2, TestParam3) {
    
    // Get test parameters for this template instantiation
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;

    // Initialize window and OpenGL context with appropriate dimensions
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;
    constexpr float TEST_GAIN = 1.0f; // Use 1.0 to match original WAV data
    const std::string test_file_path = "media/test.wav";

    // Load original WAV data for comparison
    std::vector<std::vector<float>> original_audio_data;
    try {
        original_audio_data = load_original_audio_data(test_file_path);
    } catch (const std::exception& e) {
        FAIL("Failed to load original audio data: " << e.what());
    }

    REQUIRE(!original_audio_data.empty());
    REQUIRE(!original_audio_data[0].empty());
    
    // Get the number of samples per channel from the original file
    const size_t original_samples_per_channel = original_audio_data[0].size();
    const unsigned int original_num_channels = original_audio_data.size();
    
    // Calculate how many frames we need to render to cover the entire file
    // Add some extra frames to ensure we capture everything
    const int NUM_FRAMES = static_cast<int>(std::ceil(static_cast<float>(original_samples_per_channel) / BUFFER_SIZE)) + 2;

    SECTION("Compare File Generator Output to Original WAV Data") {
        // Create file generator render stage
        AudioFileGeneratorRenderStage file_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, test_file_path);
        AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

        // Connect the generator to the final render stage
        REQUIRE(file_generator.connect_render_stage(&final_render_stage));
        
        // Add global_time parameter as a buffer parameter
        auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
        global_time_param->set_value(0);
        global_time_param->initialize();

        // Remove the envelope for clean playback (no ADSR)
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

        // Setup audio output (only if enabled)
        AudioPlayerOutput* audio_output = nullptr;
        if (is_audio_output_enabled()) {
            audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
            REQUIRE(audio_output->open());
            REQUIRE(audio_output->start());
        }

        // Play at normal speed (MIDDLE_C = 1.0x speed)
        file_generator.play_note({MIDDLE_C, TEST_GAIN});
        
        // Collect output samples per channel
        std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            output_samples_per_channel[ch].reserve(NUM_FRAMES * BUFFER_SIZE);
        }
        
        // Render frames and collect output
        for (int frame = 0; frame < NUM_FRAMES; frame++) {
            global_time_param->set_value(frame);
            global_time_param->render();

            file_generator.render(frame);
            final_render_stage.render(frame);
            
            auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
            REQUIRE(output_param != nullptr);
            
            const float* output_data = static_cast<const float*>(output_param->get_value());
            REQUIRE(output_data != nullptr);

            // Collect samples per channel
            for (int i = 0; i < BUFFER_SIZE; i++) {
                for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                    output_samples_per_channel[ch].push_back(output_data[i * NUM_CHANNELS + ch]);
                }
            }

            // Push to audio output if enabled
            if (audio_output) {
                while (!audio_output->is_ready()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                audio_output->push(output_data);
            }
        }

        // Wait for audio to finish (only if enabled)
        if (audio_output) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            audio_output->stop();
            audio_output->close();
            delete audio_output;
        }

        // Get the actual number of samples generated (may be less than expected due to tape size)
        size_t actual_output_samples = output_samples_per_channel[0].size();
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            actual_output_samples = std::min(actual_output_samples, output_samples_per_channel[ch].size());
        }
        
        // Trim both to the same size for fair comparison
        // Use the smaller of: original file size or actual output size
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            size_t min_size = std::min(original_audio_data[ch].size(), actual_output_samples);
            
            // Trim original to match output size
            if (original_audio_data[ch].size() > min_size) {
                original_audio_data[ch].resize(min_size);
            }
            
            // Trim output to match (should already be correct, but ensure it)
            if (output_samples_per_channel[ch].size() > min_size) {
                output_samples_per_channel[ch].resize(min_size);
            }
        }
        
        // Log the sizes for debugging
        std::cout << "Comparison sizes - Original: " << original_audio_data[0].size() 
                  << " samples, Output: " << output_samples_per_channel[0].size() 
                  << " samples per channel" << std::endl;

        // Compare each channel
        const int num_channels_to_compare = std::min(static_cast<int>(original_num_channels), NUM_CHANNELS);
        
        SECTION("Channel Comparison Tests") {
            for (int ch = 0; ch < num_channels_to_compare; ch++) {
                INFO("Comparing channel " << ch);
                
                const auto& original_channel = original_audio_data[ch];
                const auto& output_channel = output_samples_per_channel[ch];
                
                // Ensure both are exactly the same size
                size_t min_size = std::min(original_channel.size(), output_channel.size());
                REQUIRE(min_size > 0);
                
                // Create trimmed copies for comparison
                std::vector<float> original_trimmed(original_channel.begin(), original_channel.begin() + min_size);
                std::vector<float> output_trimmed(output_channel.begin(), output_channel.begin() + min_size);
                
                // Find best correlation with offset (cross-correlation)
                {
                    auto [best_correlation, best_offset] = find_best_correlation_with_offset(
                        original_trimmed, output_trimmed, 5000); // Search up to ~0.11 seconds at 44.1kHz
                    
                    float offset_seconds = static_cast<float>(best_offset) / SAMPLE_RATE;
                    
                    std::cout << "Channel " << ch << " - Best correlation: " << best_correlation 
                              << " at offset: " << best_offset << " samples (" 
                              << offset_seconds << " seconds)" << std::endl;
                    
                    if (best_offset != 0) {
                        std::cout << "  WARNING: Time shift detected! ";
                        if (best_offset > 0) {
                            std::cout << "Output is delayed by " << best_offset << " samples relative to original." << std::endl;
                        } else {
                            std::cout << "Output is ahead by " << -best_offset << " samples relative to original." << std::endl;
                        }
                    }
                    
                    // Correlation should be very high (close to 1.0) for accurate playback
                    // Allow some tolerance for interpolation artifacts
                    REQUIRE(best_correlation > 0.999f);
                }
                
                // RMS error test (using offset-corrected alignment)
                {
                    auto [best_correlation, best_offset] = find_best_correlation_with_offset(
                        original_trimmed, output_trimmed, 5000);
                    
                    // Create aligned vectors for RMS calculation
                    std::vector<float> aligned_original;
                    std::vector<float> aligned_output;
                    
                    if (best_offset > 0) {
                        // Output is delayed: shift original forward
                        if (static_cast<size_t>(best_offset) < original_trimmed.size()) {
                            aligned_original = std::vector<float>(original_trimmed.begin() + best_offset, original_trimmed.end());
                            size_t aligned_len = std::min(output_trimmed.size(), aligned_original.size());
                            aligned_output = std::vector<float>(output_trimmed.begin(), output_trimmed.begin() + aligned_len);
                            aligned_original.resize(aligned_len);
                        } else {
                            aligned_original.clear();
                            aligned_output.clear();
                        }
                    } else if (best_offset < 0) {
                        // Output is ahead: shift output forward
                        int abs_offset = -best_offset;
                        if (static_cast<size_t>(abs_offset) < output_trimmed.size()) {
                            aligned_output = std::vector<float>(output_trimmed.begin() + abs_offset, output_trimmed.end());
                            size_t aligned_len = std::min(original_trimmed.size(), aligned_output.size());
                            aligned_original = std::vector<float>(original_trimmed.begin(), original_trimmed.begin() + aligned_len);
                            aligned_output.resize(aligned_len);
                        } else {
                            aligned_original.clear();
                            aligned_output.clear();
                        }
                    } else {
                        aligned_original = original_trimmed;
                        aligned_output = output_trimmed;
                    }
                    
                    // Make sure both vectors are exactly the same size
                    if (!aligned_original.empty() && !aligned_output.empty()) {
                        size_t min_len = std::min(aligned_original.size(), aligned_output.size());
                        aligned_original.resize(min_len);
                        aligned_output.resize(min_len);
                    }
                    
                    float rms_error = calculate_rms_error(aligned_original, aligned_output);
                    
                    // RMS error should be low for accurate playback
                    // Allow some tolerance for interpolation artifacts and floating point precision
                    REQUIRE(rms_error < 0.1f);
                    
                    std::cout << "Channel " << ch << " RMS error (offset-corrected): " << rms_error << std::endl;
                }
                
                // Sample-by-sample comparison (using offset-corrected alignment)
                {
                    auto [best_correlation, best_offset] = find_best_correlation_with_offset(
                        original_trimmed, output_trimmed, 5000);
                    
                    // Create aligned vectors
                    std::vector<float> aligned_original;
                    std::vector<float> aligned_output;
                    
                    if (best_offset > 0) {
                        if (static_cast<size_t>(best_offset) < original_trimmed.size()) {
                            aligned_original = std::vector<float>(original_trimmed.begin() + best_offset, original_trimmed.end());
                            size_t aligned_len = std::min(output_trimmed.size(), aligned_original.size());
                            aligned_output = std::vector<float>(output_trimmed.begin(), output_trimmed.begin() + aligned_len);
                            aligned_original.resize(aligned_len);
                        } else {
                            aligned_original.clear();
                            aligned_output.clear();
                        }
                    } else if (best_offset < 0) {
                        int abs_offset = -best_offset;
                        if (static_cast<size_t>(abs_offset) < output_trimmed.size()) {
                            aligned_output = std::vector<float>(output_trimmed.begin() + abs_offset, output_trimmed.end());
                            size_t aligned_len = std::min(original_trimmed.size(), aligned_output.size());
                            aligned_original = std::vector<float>(original_trimmed.begin(), original_trimmed.begin() + aligned_len);
                            aligned_output.resize(aligned_len);
                        } else {
                            aligned_original.clear();
                            aligned_output.clear();
                        }
                    } else {
                        aligned_original = original_trimmed;
                        aligned_output = output_trimmed;
                    }
                    
                    // Ensure both are exactly the same size
                    if (!aligned_original.empty() && !aligned_output.empty()) {
                        size_t min_len = std::min(aligned_original.size(), aligned_output.size());
                        aligned_original.resize(min_len);
                        aligned_output.resize(min_len);
                    }
                    
                    if (!aligned_original.empty()) {
                        // Compare first sample
                        REQUIRE(aligned_output[0] == Catch::Approx(aligned_original[0]).margin(0.05f));
                        
                        if (aligned_original.size() > 100) {
                            // Compare a sample from the middle
                            size_t mid_sample = aligned_original.size() / 2;
                            REQUIRE(aligned_output[mid_sample] == Catch::Approx(aligned_original[mid_sample]).margin(0.05f));
                        }
                        
                        // Compare last sample
                        size_t last_sample = aligned_original.size() - 1;
                        REQUIRE(aligned_output[last_sample] == Catch::Approx(aligned_original[last_sample]).margin(0.05f));
                    }
                }
            }
        }

        // CSV output (only if enabled)
        if (is_csv_output_enabled()) {
            std::string csv_output_dir = "build/tests/csv_output";
            system(("mkdir -p " + csv_output_dir).c_str());
            
            std::ostringstream filename_stream;
            filename_stream << csv_output_dir << "/file_generator_wav_comparison_buffer_" 
                          << BUFFER_SIZE << "_channels_" << NUM_CHANNELS << ".csv";
            std::string filename = filename_stream.str();
            
            CSVTestOutput csv_writer(filename, SAMPLE_RATE);
            REQUIRE(csv_writer.is_open());
            
            csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
            csv_writer.close();
            
            std::cout << "Wrote file generator comparison output to " << filename << " (" 
                      << output_samples_per_channel[0].size() << " samples, " << NUM_CHANNELS << " channels)" << std::endl;
        }

        // Stop the note
        file_generator.stop_note(MIDDLE_C);

        final_render_stage.unbind();
        file_generator.unbind();
        delete global_time_param;
    }
}

TEMPLATE_TEST_CASE("AudioFileGeneratorRenderStage - WAV File Speed Comparison Test", "[audio_file_generator_render_stage][gl_test][audio_output][csv_output][template]",
                   TestParam1, TestParam2, TestParam3) {
    
    // Get test parameters for this template instantiation
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;

    // Initialize window and OpenGL context with appropriate dimensions
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;
    constexpr float TEST_GAIN = 1.0f; // Use 1.0 to match original WAV data
    const std::string test_file_path = "media/test.wav";

    // Load original WAV data for comparison
    std::vector<std::vector<float>> original_audio_data;
    try {
        original_audio_data = load_original_audio_data(test_file_path);
    } catch (const std::exception& e) {
        FAIL("Failed to load original audio data: " << e.what());
    }

    REQUIRE(!original_audio_data.empty());
    REQUIRE(!original_audio_data[0].empty());
    
    // Get the number of samples per channel from the original file
    const size_t original_samples_per_channel = original_audio_data[0].size();
    const unsigned int original_num_channels = original_audio_data.size();
    
    // Calculate how many frames we need to render to cover the entire file
    // Add some extra frames to ensure we capture everything
    const int NUM_FRAMES = static_cast<int>(std::ceil(static_cast<float>(original_samples_per_channel) / BUFFER_SIZE)) + 2;

    SECTION("Compare File Generator Output at Different Speeds") {
        // Test different playback speeds: 0.5x, 1.0x, and 2.0x
        struct SpeedTest {
            float speed_ratio;
            float note_frequency;
            const char* name;
            float min_correlation; // Minimum expected correlation
        };
        
        const SpeedTest speed_tests[] = {
            {0.5f, MIDDLE_C * 0.5f, "Half Speed (0.5x)", 0.99f},
            {1.0f, MIDDLE_C, "Normal Speed (1.0x)", 0.999f},
            {2.0f, MIDDLE_C * 2.0f, "Double Speed (2.0x)", 0.99f}
        };
        
        for (const auto& speed_test : speed_tests) {
            INFO("Testing " << speed_test.name);
            
            // Create file generator render stage
            AudioFileGeneratorRenderStage file_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, test_file_path);
            AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
            
            // Connect the generator to the final render stage
            REQUIRE(file_generator.connect_render_stage(&final_render_stage));
            
            // Add global_time parameter
            auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
            global_time_param->set_value(0);
            global_time_param->initialize();
            
            // Remove the envelope for clean playback (no ADSR)
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
            
            // Play at the specified speed
            file_generator.play_note({speed_test.note_frequency, TEST_GAIN});
            
            // Collect output samples per channel
            std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                output_samples_per_channel[ch].reserve(NUM_FRAMES * BUFFER_SIZE);
            }
            
            // Render frames and collect output
            for (int frame = 0; frame < NUM_FRAMES; frame++) {
                global_time_param->set_value(frame);
                global_time_param->render();
                
                file_generator.render(frame);
                final_render_stage.render(frame);
                
                auto output_param = final_render_stage.find_parameter("final_output_audio_texture");
                REQUIRE(output_param != nullptr);
                
                const float* output_data = static_cast<const float*>(output_param->get_value());
                REQUIRE(output_data != nullptr);
                
                // Collect samples per channel
                for (int i = 0; i < BUFFER_SIZE; i++) {
                    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                        output_samples_per_channel[ch].push_back(output_data[i * NUM_CHANNELS + ch]);
                    }
                }
            }
            
            // Resample original audio to match the speed
            std::vector<std::vector<float>> resampled_original(NUM_CHANNELS);
            for (int ch = 0; ch < NUM_CHANNELS && ch < static_cast<int>(original_audio_data.size()); ch++) {
                resampled_original[ch] = resample_audio(original_audio_data[ch], speed_test.speed_ratio);
            }
            
            // Compare each channel
            const int num_channels_to_compare = std::min(static_cast<int>(original_num_channels), NUM_CHANNELS);
            
            for (int ch = 0; ch < num_channels_to_compare; ch++) {
                INFO("Comparing channel " << ch << " at " << speed_test.name);
                
                const auto& resampled_channel = resampled_original[ch];
                const auto& output_channel = output_samples_per_channel[ch];
                
                // Ensure both are the same size (trim to smaller)
                size_t min_size = std::min(resampled_channel.size(), output_channel.size());
                REQUIRE(min_size > 0);
                
                // Create trimmed copies for comparison
                std::vector<float> resampled_trimmed(resampled_channel.begin(), resampled_channel.begin() + min_size);
                std::vector<float> output_trimmed(output_channel.begin(), output_channel.begin() + min_size);
                
                // Find best correlation with offset
                auto [best_correlation, best_offset] = find_best_correlation_with_offset(
                    resampled_trimmed, output_trimmed, 5000);
                
                float offset_seconds = static_cast<float>(best_offset) / SAMPLE_RATE;
                
                std::cout << "Channel " << ch << " at " << speed_test.name 
                          << " - Best correlation: " << best_correlation 
                          << " at offset: " << best_offset << " samples (" 
                          << offset_seconds << " seconds)" << std::endl;
                
                // Check correlation meets minimum threshold
                REQUIRE(best_correlation > speed_test.min_correlation);
                
                // Also check RMS error (using offset-corrected alignment)
                std::vector<float> aligned_resampled;
                std::vector<float> aligned_output;
                
                if (best_offset > 0) {
                    // Output is delayed: shift resampled forward
                    if (static_cast<size_t>(best_offset) < resampled_trimmed.size()) {
                        aligned_resampled = std::vector<float>(resampled_trimmed.begin() + best_offset, resampled_trimmed.end());
                        size_t aligned_len = std::min(output_trimmed.size(), aligned_resampled.size());
                        aligned_output = std::vector<float>(output_trimmed.begin(), output_trimmed.begin() + aligned_len);
                        aligned_resampled.resize(aligned_len);
                    }
                } else if (best_offset < 0) {
                    // Output is ahead: shift output forward
                    int abs_offset = -best_offset;
                    if (static_cast<size_t>(abs_offset) < output_trimmed.size()) {
                        aligned_output = std::vector<float>(output_trimmed.begin() + abs_offset, output_trimmed.end());
                        size_t aligned_len = std::min(resampled_trimmed.size(), aligned_output.size());
                        aligned_resampled = std::vector<float>(resampled_trimmed.begin(), resampled_trimmed.begin() + aligned_len);
                        aligned_output.resize(aligned_len);
                    }
                } else {
                    aligned_resampled = resampled_trimmed;
                    aligned_output = output_trimmed;
                }
                
                if (!aligned_resampled.empty() && !aligned_output.empty()) {
                    size_t min_len = std::min(aligned_resampled.size(), aligned_output.size());
                    aligned_resampled.resize(min_len);
                    aligned_output.resize(min_len);
                    
                    float rms_error = calculate_rms_error(aligned_resampled, aligned_output);
                    std::cout << "Channel " << ch << " at " << speed_test.name 
                              << " RMS error (offset-corrected): " << rms_error << std::endl;
                    
                    // RMS error should be reasonable (allow more tolerance for speed changes)
                    REQUIRE(rms_error < 0.15f);
                }
            }
            
            // Stop the note
            file_generator.stop_note(speed_test.note_frequency);
            
            final_render_stage.unbind();
            file_generator.unbind();
            delete global_time_param;
        }
    }
}

TEST_CASE("AudioGeneratorRenderStage - Note State Transfer on Connect/Disconnect", "[audio_generator_render_stage][gl_test][note_state_transfer]") {
    constexpr int BUFFER_SIZE = 512;
    constexpr int NUM_CHANNELS = 2;
    constexpr int SAMPLE_RATE = 44100;

    // Initialize window and OpenGL context
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    // Create two generator render stages
    AudioGeneratorRenderStage generator1(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 
                                        "build/shaders/multinote_sine_generator_render_stage.glsl");
    AudioGeneratorRenderStage generator2(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 
                                        "build/shaders/multinote_sine_generator_render_stage.glsl");
    AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Initialize all stages
    REQUIRE(generator1.initialize());
    REQUIRE(generator2.initialize());
    REQUIRE(final_render_stage.initialize());

    // Connect generator1 to final render stage
    REQUIRE(generator1.connect_render_stage(&final_render_stage));

    context.prepare_draw();
    REQUIRE(generator1.bind());
    REQUIRE(final_render_stage.bind());

    // Add global_time parameter
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();

    // Set envelope parameters for immediate response
    auto attack_param1 = generator1.find_parameter("attack_time");
    attack_param1->set_value(0.0f);
    auto release_param1 = generator1.find_parameter("release_time");
    release_param1->set_value(0.1f); // Short release for testing

    // Play multiple notes on generator1
    const float note1 = 261.63f; // C4
    const float note2 = 293.66f; // D4
    const float note3 = 329.63f; // E4
    const float gain = 0.5f;

    generator1.play_note({note1, gain});
    generator1.play_note({note2, gain});
    generator1.play_note({note3, gain});

    // Render a few frames to let notes start playing
    for (int frame = 0; frame < 5; frame++) {
        global_time_param->set_value(frame);
        global_time_param->render();
        generator1.render(frame);
        final_render_stage.render(frame);
    }

    // Verify generator1 has 3 active notes
    auto active_notes_param1 = generator1.find_parameter("active_notes");
    REQUIRE(active_notes_param1 != nullptr);
    int active_notes1 = *(int*)active_notes_param1->get_value();
    REQUIRE(active_notes1 == 3);

    // Stop one note (note2) - this should mark it as stopped but not delete it yet
    generator1.stop_note(note2);

    // Render a few more frames
    for (int frame = 5; frame < 10; frame++) {
        global_time_param->set_value(frame);
        global_time_param->render();
        generator1.render(frame);
        final_render_stage.render(frame);
    }

    // Verify generator1 still has 3 notes (one stopped, two playing)
    active_notes1 = *(int*)active_notes_param1->get_value();
    REQUIRE(active_notes1 == 3);

    // Get stop_positions to verify one note is stopped
    auto stop_positions_param1 = generator1.find_parameter("stop_positions");
    REQUIRE(stop_positions_param1 != nullptr);
    int* stop_positions1 = (int*)stop_positions_param1->get_value();
    
    // Find which note is stopped (should have stop_position != -1)
    int stopped_count = 0;
    int playing_count = 0;
    for (int i = 0; i < active_notes1; i++) {
        if (stop_positions1[i] == -1) {
            playing_count++;
        } else {
            stopped_count++;
        }
    }
    REQUIRE(stopped_count == 1); // One note should be stopped
    REQUIRE(playing_count == 2); // Two notes should still be playing

    // Disconnect generator1 - should upload only actively playing notes to clipboard
    REQUIRE(generator1.disconnect_render_stage(&final_render_stage));

    // Verify generator1's note state is cleared
    active_notes1 = *(int*)active_notes_param1->get_value();
    REQUIRE(active_notes1 == 0);

    // Connect generator2 - should download notes from clipboard
    REQUIRE(generator2.connect_render_stage(&final_render_stage));

    // Set envelope parameters for generator2
    auto attack_param2 = generator2.find_parameter("attack_time");
    attack_param2->set_value(0.0f);
    auto release_param2 = generator2.find_parameter("release_time");
    release_param2->set_value(0.1f);

    REQUIRE(generator2.bind());

    // Verify generator2 has 2 active notes (only the playing ones, not the stopped one)
    auto active_notes_param2 = generator2.find_parameter("active_notes");
    REQUIRE(active_notes_param2 != nullptr);
    int active_notes2 = *(int*)active_notes_param2->get_value();
    REQUIRE(active_notes2 == 2); // Only actively playing notes should be transferred

    // Verify the notes are the correct ones (note1 and note3, not note2)
    auto tones_param2 = generator2.find_parameter("tones");
    REQUIRE(tones_param2 != nullptr);
    float* tones2 = (float*)tones_param2->get_value();
    
    bool found_note1 = false;
    bool found_note3 = false;
    bool found_note2 = false;
    
    for (int i = 0; i < active_notes2; i++) {
        if (std::abs(tones2[i] - note1) < 0.01f) {
            found_note1 = true;
        } else if (std::abs(tones2[i] - note3) < 0.01f) {
            found_note3 = true;
        } else if (std::abs(tones2[i] - note2) < 0.01f) {
            found_note2 = true;
        }
    }
    
    REQUIRE(found_note1);
    REQUIRE(found_note3);
    REQUIRE(!found_note2); // Stopped note should not be transferred

    // Verify all transferred notes are still playing (stop_positions == -1)
    auto stop_positions_param2 = generator2.find_parameter("stop_positions");
    REQUIRE(stop_positions_param2 != nullptr);
    int* stop_positions2 = (int*)stop_positions_param2->get_value();
    
    for (int i = 0; i < active_notes2; i++) {
        REQUIRE(stop_positions2[i] == -1); // All transferred notes should be playing
    }

    // Render a few frames with generator2 to verify it works
    for (int frame = 10; frame < 15; frame++) {
        global_time_param->set_value(frame);
        global_time_param->render();
        generator2.render(frame);
        final_render_stage.render(frame);
    }

    // Verify clipboard is cleared after download
    // When generator2 connected, it downloaded notes and cleared the clipboard.
    // Now verify that when generator2 disconnects, it uploads its current notes.
    // Then when generator3 connects, it should download those notes and clear the clipboard again.
    
    // Clear generator2's notes directly to test that empty state is uploaded
    while (generator2.m_note_state.m_active_notes > 0) {
        generator2.m_note_state.delete_note(0);
    }
    generator2.m_note_state.set_parameters(&generator2);
    
    // Verify generator2 has no active notes now
    active_notes2 = *(int*)active_notes_param2->get_value();
    REQUIRE(active_notes2 == 0);
    
    // Disconnect generator2 - should upload empty state (0 notes)
    REQUIRE(generator2.disconnect_render_stage(&final_render_stage));
    
    // Connect generator3 - should download 0 notes (empty clipboard)
    AudioGeneratorRenderStage generator3(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 
                                        "build/shaders/multinote_sine_generator_render_stage.glsl");
    REQUIRE(generator3.initialize());
    REQUIRE(generator3.connect_render_stage(&final_render_stage));
    
    auto active_notes_param3 = generator3.find_parameter("active_notes");
    REQUIRE(active_notes_param3 != nullptr);
    int active_notes3 = *(int*)active_notes_param3->get_value();
    REQUIRE(active_notes3 == 0); // Clipboard should be empty (generator2 uploaded empty state)

    // Cleanup
    generator1.unbind();
    generator2.unbind();
    generator3.unbind();
    final_render_stage.unbind();
    delete global_time_param;
}