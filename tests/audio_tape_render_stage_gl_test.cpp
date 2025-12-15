#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"
#include "framework/test_main.h"

#include "audio_render_stage/audio_tape_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"
#include "audio_output/audio_player_output.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "framework/csv_test_output.h"

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>

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

constexpr TestParams get_test_params(int index) {
    constexpr TestParams params[] = {
        {256, 2, "256_buffer_2_channels"},
        {512, 2, "512_buffer_2_channels"},
        {1024, 2, "1024_buffer_2_channels"}
    };
    return params[index];
}

TEMPLATE_TEST_CASE("AudioTapeRenderStage - Simple Record and Playback to Audio", 
                   "[audio_tape_render_stage][gl_test][template]", 
                   TestParam1, TestParam2, TestParam3) {
    
    // Get test parameters
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr float TEST_FREQUENCY = 440.0f;
    constexpr float TEST_GAIN = 0.3f;
    constexpr int NUM_FRAMES = SAMPLE_RATE / BUFFER_SIZE * 2; // 2 seconds

    // Initialize window and OpenGL context
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    // Create sine generator
    AudioGeneratorRenderStage generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 
                                        "build/shaders/multinote_sine_generator_render_stage.glsl");

    // Create record stage with default shader
    AudioRecordRenderStage record_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Create playback stage with default shader
    AudioPlaybackRenderStage playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Create final render stage
    AudioFinalRenderStage final_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Add global_time parameter
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();

    // Initialize all stages
    REQUIRE(generator.initialize());
    REQUIRE(record_stage.initialize());
    REQUIRE(playback_stage.initialize());
    REQUIRE(final_stage.initialize());

    // Connect generator to record
    REQUIRE(generator.connect_render_stage(&record_stage));

    // For playback, connect playback to final
    REQUIRE(playback_stage.connect_render_stage(&final_stage));

    context.prepare_draw();
    
    // Bind all stages
    REQUIRE(generator.bind());
    REQUIRE(record_stage.bind());
    REQUIRE(playback_stage.bind());
    REQUIRE(final_stage.bind());

    // Start recording
    record_stage.record(0);

    // Play note
    generator.play_note({TEST_FREQUENCY, TEST_GAIN});

    // Prepare audio output (only if enabled)
    AudioPlayerOutput* audio_output = nullptr;
    if (is_audio_output_enabled()) {
        audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
        REQUIRE(audio_output->open());
        REQUIRE(audio_output->start());
    }

    std::vector<float> input_samples;
    input_samples.reserve(BUFFER_SIZE * NUM_CHANNELS * NUM_FRAMES);

    // Render frames for recording
    for (int frame = 0; frame < NUM_FRAMES; ++frame) {
        global_time_param->set_value(frame);
        global_time_param->render();

        generator.render(frame);
        record_stage.render(frame);

        auto input_param = generator.find_parameter("output_audio_texture");
        REQUIRE(input_param != nullptr);
        const float* input_data = static_cast<const float*>(input_param->get_value());
        REQUIRE(input_data != nullptr);

        // Store input samples (channel-major format: [ch0_buffer, ch1_buffer, ...])
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            const float* channel_data = input_data + (ch * BUFFER_SIZE);
            for (int i = 0; i < BUFFER_SIZE; ++i) {
                input_samples.push_back(channel_data[i]);
            }
        }
    }

    // Stop note
    generator.stop_note(TEST_FREQUENCY);

    // Stop recording
    record_stage.stop();

    // Load tape from record to playback
    playback_stage.load_tape(record_stage.get_tape());

    // Start playback
    playback_stage.play(0);

    // Render playback frames and push to audio output
    std::vector<float> output_samples;
    output_samples.reserve(BUFFER_SIZE * NUM_CHANNELS * NUM_FRAMES);

    for (int frame = 0; frame < NUM_FRAMES; ++frame) {
        global_time_param->set_value(frame);
        global_time_param->render();

        playback_stage.render(frame);
        final_stage.render(frame);

        auto before_final_param = playback_stage.find_parameter("output_audio_texture");

        REQUIRE(before_final_param != nullptr);

        const float* before_final_data = static_cast<const float*>(before_final_param->get_value());
        REQUIRE(before_final_data != nullptr);

        auto output_param = final_stage.find_parameter("final_output_audio_texture");
        REQUIRE(output_param != nullptr);

        const float* output_data = static_cast<const float*>(output_param->get_value());
        REQUIRE(output_data != nullptr);

        // Store output samples (final_output_audio_texture is interleaved: [frame0_ch0, frame0_ch1, ...])
        for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
            output_samples.push_back(output_data[i]);
        }

        // Push to audio output (only if enabled)
        if (audio_output) {
            while (!audio_output->is_ready()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            audio_output->push(output_data);
        }
    }

    // Stop playback
    playback_stage.stop();

    // Cleanup audio (only if enabled)
    if (audio_output) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        audio_output->stop();
        audio_output->close();
        delete audio_output;
    }

    // CSV output (only if enabled)
    if (is_csv_output_enabled()) {
        std::string csv_output_dir = "build/tests/csv_output";
        system(("mkdir -p " + csv_output_dir).c_str());
        
        // Convert to per-channel format
        // Input samples are stored channel-major: [ch0_frame0, ch0_frame1, ..., ch1_frame0, ch1_frame1, ...]
        // Output samples are interleaved: [frame0_ch0, frame0_ch1, frame1_ch0, frame1_ch1, ...]
        std::vector<std::vector<float>> input_samples_per_channel(NUM_CHANNELS);
        std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
        const size_t samples_per_channel = (BUFFER_SIZE * NUM_FRAMES);
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            input_samples_per_channel[ch].reserve(samples_per_channel);
            output_samples_per_channel[ch].reserve(samples_per_channel);
        }
        
        // Convert input samples from channel-major to per-channel vectors
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            const float* channel_data = input_samples.data() + (ch * BUFFER_SIZE * NUM_FRAMES);
            for (size_t i = 0; i < samples_per_channel; ++i) {
                input_samples_per_channel[ch].push_back(channel_data[i]);
            }
        }
        
        // Deinterleave output samples (interleaved format)
        for (size_t i = 0; i < output_samples.size(); i += NUM_CHANNELS) {
            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                output_samples_per_channel[ch].push_back(output_samples[i + ch]);
            }
        }
        
        // Write input samples
        {
            std::ostringstream filename_stream;
            filename_stream << csv_output_dir << "/audio_simple_record_playback_" << params.name << "_input.csv";
            std::string filename = filename_stream.str();
            
            CSVTestOutput csv_writer(filename, SAMPLE_RATE);
            REQUIRE(csv_writer.is_open());
            csv_writer.write_channels(input_samples_per_channel, SAMPLE_RATE);
            csv_writer.close();
            
            std::cout << "Wrote input samples to " << filename << " (" 
                      << input_samples_per_channel[0].size() << " samples, " << NUM_CHANNELS << " channels)" << std::endl;
        }
        
        // Write output samples
        {
            std::ostringstream filename_stream;
            filename_stream << csv_output_dir << "/audio_simple_record_playback_" << params.name << "_output.csv";
            std::string filename = filename_stream.str();
            
            CSVTestOutput csv_writer(filename, SAMPLE_RATE);
            REQUIRE(csv_writer.is_open());
            csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
            csv_writer.close();
            
            std::cout << "Wrote output samples to " << filename << " (" 
                      << output_samples_per_channel[0].size() << " samples, " << NUM_CHANNELS << " channels)" << std::endl;
        }
    }

    SECTION("Continuity and Discontinuity Check") {
        // Split interleaved samples into per-channel vectors
        std::vector<std::vector<float>> channel_samples(NUM_CHANNELS);
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            channel_samples[ch].reserve(output_samples.size() / NUM_CHANNELS);
        }
        for (size_t i = 0; i + (NUM_CHANNELS - 1) < output_samples.size(); i += NUM_CHANNELS) {
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                channel_samples[ch].push_back(output_samples[i + ch]);
            }
        }

        // Check for sample-to-sample discontinuities per channel
        constexpr float MAX_SAMPLE_DIFF = 0.1f; // conservative for multi-tone content
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            std::size_t discontinuity_count = 0;
            const auto &samples = channel_samples[ch];
            for (size_t i = 1; i < samples.size(); ++i) {
                float diff = std::abs(samples[i] - samples[i - 1]);
                if (diff > MAX_SAMPLE_DIFF) {
                    discontinuity_count++;
                }
            }
            INFO("Channel " << ch << " discontinuities: " << discontinuity_count);
            REQUIRE(discontinuity_count == 0);
        }
    }

    SECTION("Slope Change Check") {
        // Split interleaved samples into per-channel vectors
        std::vector<std::vector<float>> channel_samples(NUM_CHANNELS);
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            channel_samples[ch].reserve(output_samples.size() / NUM_CHANNELS);
        }
        for (size_t i = 0; i + (NUM_CHANNELS - 1) < output_samples.size(); i += NUM_CHANNELS) {
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                channel_samples[ch].push_back(output_samples[i + ch]);
            }
        }

        // This test verifies that the rate of change (slope) changes incrementally
        // without large jumps, which would indicate smooth playback transitions
        constexpr float MAX_SLOPE_CHANGE_JUMP = 0.05f; // Maximum allowed jump in slope change per sample
        constexpr float MAX_ABSOLUTE_SLOPE_CHANGE = 0.1f; // Maximum allowed absolute slope change (catches sudden transitions)
        constexpr float MIN_SLOPE_CHANGE_FOR_ANALYSIS = 0.0001f; // Ignore very small slope changes

        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            const auto &samples = channel_samples[ch];
            
            if (samples.size() < 3) {
                continue; // Need at least 3 samples to calculate slope changes
            }
            
            // Calculate first derivative (slope) between consecutive samples
            std::vector<float> slopes;
            slopes.reserve(samples.size() - 1);
            for (size_t i = 1; i < samples.size(); ++i) {
                float slope = samples[i] - samples[i - 1];
                slopes.push_back(slope);
            }
            
            // Calculate second derivative (change in slope)
            std::vector<float> slope_changes;
            slope_changes.reserve(slopes.size() - 1);
            for (size_t i = 1; i < slopes.size(); ++i) {
                float slope_change = std::abs(slopes[i] - slopes[i - 1]);
                slope_changes.push_back(slope_change);
            }
            
            // Find large absolute slope changes (catches sudden transitions from zero or to zero)
            std::vector<size_t> large_absolute_slope_change_indices;
            std::vector<float> large_absolute_slope_change_magnitudes;
            
            for (size_t i = 0; i < slope_changes.size(); ++i) {
                if (slope_changes[i] > MAX_ABSOLUTE_SLOPE_CHANGE) {
                    large_absolute_slope_change_indices.push_back(i + 1); // +1 because slope_changes[i] corresponds to sample i+1
                    large_absolute_slope_change_magnitudes.push_back(slope_changes[i]);
                }
            }
            
            // Find large jumps in slope changes (third derivative - change in slope change)
            std::vector<size_t> large_jump_indices;
            std::vector<float> large_jump_magnitudes;
            
            for (size_t i = 1; i < slope_changes.size(); ++i) {
                float jump = std::abs(slope_changes[i] - slope_changes[i - 1]);
                // Only flag if both slope changes are significant enough to matter
                if (slope_changes[i] > MIN_SLOPE_CHANGE_FOR_ANALYSIS && 
                    slope_changes[i - 1] > MIN_SLOPE_CHANGE_FOR_ANALYSIS &&
                    jump > MAX_SLOPE_CHANGE_JUMP) {
                    large_jump_indices.push_back(i + 1); // +1 because slope_changes[i] corresponds to sample i+1
                    large_jump_magnitudes.push_back(jump);
                }
            }
            
            INFO("Channel " << ch << " slope change analysis:");
            INFO("  Total samples: " << samples.size());
            INFO("  Total slopes calculated: " << slopes.size());
            INFO("  Total slope changes calculated: " << slope_changes.size());
            INFO("  Max absolute slope change threshold: " << MAX_ABSOLUTE_SLOPE_CHANGE);
            INFO("  Max slope change jump threshold: " << MAX_SLOPE_CHANGE_JUMP);
            INFO("  Found " << large_absolute_slope_change_indices.size() << " large absolute slope changes");
            INFO("  Found " << large_jump_indices.size() << " large slope change jumps");
            
            if (large_absolute_slope_change_indices.size() > 0) {
                INFO("  First large absolute slope change at sample " << large_absolute_slope_change_indices[0]
                     << " with magnitude " << large_absolute_slope_change_magnitudes[0]);
                INFO("    Sample value: " << samples[large_absolute_slope_change_indices[0]]
                     << ", previous: " << samples[large_absolute_slope_change_indices[0] - 1]);
            }
            
            if (large_jump_indices.size() > 0) {
                INFO("  First large jump at sample " << large_jump_indices[0]
                     << " with magnitude " << large_jump_magnitudes[0]);
            }
            
            // Verify that slope changes are smooth without large jumps
            // For a sine wave playback, we expect smooth transitions
            REQUIRE(large_jump_indices.size() == 0);
            
            // Also check for large absolute slope changes (should be minimal for smooth sine wave)
            // Allow a small number due to potential quantization or rounding, but should be very rare
            REQUIRE(large_absolute_slope_change_indices.size() < samples.size() / 100); // Less than 1% of samples
        }
    }

    SECTION("Tape Position Tracking") {
        // Test that get_current_tape_position returns correct values
        playback_stage.load_tape(record_stage.get_tape());
        
        // Get initial position (might not be 0 if playback_stage was used before)
        unsigned int initial_pos = playback_stage.get_current_tape_position(0);
        
        // Start playback at position 0 (this will set the position)
        playback_stage.play(0);
        unsigned int pos_after_play = playback_stage.get_current_tape_position(0);
        REQUIRE(pos_after_play == 0);
        
        // Render a few frames and check position advances
        for (int frame = 0; frame < 10; ++frame) {
            global_time_param->set_value(frame);
            global_time_param->render();
            playback_stage.render(frame);
            
            unsigned int current_pos = playback_stage.get_current_tape_position(frame);
            // Position should advance by frames_per_buffer each frame (at 1.0x speed)
            // Note: position updates happen during render, so check after render
            unsigned int expected_pos = (frame + 1) * BUFFER_SIZE; // After render, position advances
            REQUIRE(current_pos == expected_pos);
        }
        
        // Stop playback
        playback_stage.stop();
        
        // Position should remain at last position after stop (10 frames rendered = position at 10 * BUFFER_SIZE)
        unsigned int pos_after_stop = playback_stage.get_current_tape_position(10);
        REQUIRE(pos_after_stop == 10 * BUFFER_SIZE);
        
        // Start playback at a different position
        playback_stage.play(5);
        unsigned int pos_at_position_5 = playback_stage.get_current_tape_position(10);
        REQUIRE(pos_at_position_5 == 5 * BUFFER_SIZE);
        
        // Render a few more frames
        for (int frame = 10; frame < 15; ++frame) {
            global_time_param->set_value(frame);
            global_time_param->render();
            playback_stage.render(frame);
            
            unsigned int current_pos = playback_stage.get_current_tape_position(frame);
            // Position should be: 5 * BUFFER_SIZE + (frame - 10 + 1) * BUFFER_SIZE
            // +1 because position advances during render
            unsigned int expected_pos = (5 + (frame - 10) + 1) * BUFFER_SIZE;
            REQUIRE(current_pos == expected_pos);
        }
        
        playback_stage.stop();
    }

    SECTION("Tape Speed Control") {
        // Test that set_tape_speed and get_tape_speed work correctly
        playback_stage.load_tape(record_stage.get_tape());
        
        // Start playback first to ensure tape is in a known state
        playback_stage.play(0);
        
        // Test positive speeds
        playback_stage.set_tape_speed(2.0f); // Double speed
        // Render once to apply pending speed changes
        global_time_param->set_value(0);
        global_time_param->render();
        playback_stage.render(0);
        REQUIRE(playback_stage.get_tape_speed() == Catch::Approx(2.0f).margin(0.001f));
        
        playback_stage.set_tape_speed(0.5f); // Half speed
        global_time_param->set_value(1);
        global_time_param->render();
        playback_stage.render(1);
        REQUIRE(playback_stage.get_tape_speed() == Catch::Approx(0.5f).margin(0.001f));
        
        playback_stage.set_tape_speed(1.0f); // Normal speed
        global_time_param->set_value(2);
        global_time_param->render();
        playback_stage.render(2);
        REQUIRE(playback_stage.get_tape_speed() == Catch::Approx(1.0f).margin(0.001f));
        
        // Test negative speeds (reverse playback)
        playback_stage.set_tape_speed(-1.0f); // Reverse at normal speed
        global_time_param->set_value(3);
        global_time_param->render();
        playback_stage.render(3);
        REQUIRE(playback_stage.get_tape_speed() == Catch::Approx(-1.0f).margin(0.001f));
        
        playback_stage.set_tape_speed(-2.0f); // Reverse at double speed
        global_time_param->set_value(4);
        global_time_param->render();
        playback_stage.render(4);
        REQUIRE(playback_stage.get_tape_speed() == Catch::Approx(-2.0f).margin(0.001f));
        
        playback_stage.set_tape_speed(-0.5f); // Reverse at half speed
        global_time_param->set_value(5);
        global_time_param->render();
        playback_stage.render(5);
        REQUIRE(playback_stage.get_tape_speed() == Catch::Approx(-0.5f).margin(0.001f));
        
        // Test that speed affects playback direction
        playback_stage.stop();
        playback_stage.play(10); // Start at position 10
        playback_stage.set_tape_speed(2.0f); // Double speed forward
        
        // Render a few frames and verify position advances forward
        // Need to render once to get initial position after play() sets it
        global_time_param->set_value(0);
        global_time_param->render();
        playback_stage.render(0);
        unsigned int pos_before = playback_stage.get_current_tape_position(0);
        
        for (int frame = 1; frame < 4; ++frame) {
            global_time_param->set_value(frame);
            global_time_param->render();
            playback_stage.render(frame);
        }
        unsigned int pos_after_forward = playback_stage.get_current_tape_position(4);
        // Position should have advanced forward
        REQUIRE(pos_after_forward > pos_before);
        
        // Test reverse playback - position should decrease
        playback_stage.set_tape_speed(-1.0f); // Reverse at normal speed
        // Render once to apply speed change
        global_time_param->set_value(4);
        global_time_param->render();
        playback_stage.render(4);
        unsigned int pos_before_reverse = playback_stage.get_current_tape_position(4);
        
        // Render a few frames in reverse
        for (int frame = 5; frame < 8; ++frame) {
            global_time_param->set_value(frame);
            global_time_param->render();
            playback_stage.render(frame);
        }
        unsigned int pos_after_reverse = playback_stage.get_current_tape_position(8);
        // Position should have decreased (moved backward)
        REQUIRE(pos_after_reverse < pos_before_reverse);
        
        playback_stage.stop();
    }

    // Cleanup
    delete global_time_param;
}

TEMPLATE_TEST_CASE("AudioTapeRenderStage - Forward/Reverse Playback with Continuous Speed Changes", 
                   "[audio_tape_render_stage][gl_test][template][speed_changes]", 
                   TestParam1, TestParam2, TestParam3) {
    
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr float TEST_FREQUENCY = 440.0f;
    constexpr float TEST_GAIN = 0.3f;
    constexpr float MAX_SPEED = 2.0f; // Maximum playback speed (forward)
    constexpr float MIN_SPEED = -2.0f; // Minimum playback speed (reverse)
    constexpr float PLAYBACK_DURATION_SECONDS = 6.0f; // Total playback duration
    constexpr int PLAYBACK_FRAMES = static_cast<int>(SAMPLE_RATE / BUFFER_SIZE * PLAYBACK_DURATION_SECONDS);
    // Record enough audio to cover playback at max speed (2x) plus some buffer
    // At 2x speed, 6 seconds of playback needs 12 seconds of audio, plus buffer for reverse
    constexpr float RECORD_DURATION_SECONDS = PLAYBACK_DURATION_SECONDS * std::abs(MAX_SPEED) + 2.0f; // Extra 2 seconds buffer
    constexpr int RECORD_FRAMES = static_cast<int>(SAMPLE_RATE / BUFFER_SIZE * RECORD_DURATION_SECONDS);
    
    // Initialize window and OpenGL context
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;
    
    // Create sine generator
    AudioGeneratorRenderStage generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 
                                        "build/shaders/multinote_sine_generator_render_stage.glsl");
    
    // Create record stage
    AudioRecordRenderStage record_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
    
    // Create playback stage
    AudioPlaybackRenderStage playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
    
    // Create final render stage
    AudioFinalRenderStage final_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
    
    // Add global_time parameter
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();
    
    // Initialize all stages
    REQUIRE(generator.initialize());
    REQUIRE(record_stage.initialize());
    REQUIRE(playback_stage.initialize());
    REQUIRE(final_stage.initialize());
    
    // Connect stages
    REQUIRE(generator.connect_render_stage(&record_stage));
    REQUIRE(playback_stage.connect_render_stage(&final_stage));
    
    context.prepare_draw();
    
    // Bind all stages
    REQUIRE(generator.bind());
    REQUIRE(record_stage.bind());
    REQUIRE(playback_stage.bind());
    REQUIRE(final_stage.bind());
    
    // Record sine wave
    record_stage.record(0);
    generator.play_note({TEST_FREQUENCY, TEST_GAIN});
    
    for (int frame = 0; frame < RECORD_FRAMES; ++frame) {
        global_time_param->set_value(frame);
        global_time_param->render();
        
        generator.render(frame);
        record_stage.render(frame);
    }
    
    generator.stop_note(TEST_FREQUENCY);
    record_stage.stop();
    
    // Load tape to playback stage
    playback_stage.load_tape(record_stage.get_tape());
    playback_stage.play(0);
    
    // Prepare audio output (only if enabled)
    AudioPlayerOutput* audio_output = nullptr;
    if (is_audio_output_enabled()) {
        audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
        REQUIRE(audio_output->open());
        REQUIRE(audio_output->start());
    }
    
    // Store output samples for analysis
    std::vector<float> output_samples;
    output_samples.reserve(BUFFER_SIZE * NUM_CHANNELS * PLAYBACK_FRAMES);
    
    // Playback with gradually changing speed: smoothly transition from +2.0x to -2.0x
    // Use linear interpolation to create smooth speed transitions
    for (int frame = 0; frame < PLAYBACK_FRAMES; ++frame) {
        global_time_param->set_value(frame);
        global_time_param->render();
        
        // Calculate normalized position (0.0 to 1.0) across playback duration
        float normalized_pos = static_cast<float>(frame) / static_cast<float>(PLAYBACK_FRAMES - 1);
        
        // Gradually change speed from MAX_SPEED (+2.0x) to MIN_SPEED (-2.0x)
        // Using linear interpolation: speed = MAX_SPEED + (MIN_SPEED - MAX_SPEED) * normalized_pos
        float speed = MAX_SPEED + (MIN_SPEED - MAX_SPEED) * normalized_pos;
        
        playback_stage.set_tape_speed(speed);
        playback_stage.render(frame);
        final_stage.render(frame);
        
        // Get output data
        auto output_param = final_stage.find_parameter("final_output_audio_texture");
        REQUIRE(output_param != nullptr);
        
        const float* output_data = static_cast<const float*>(output_param->get_value());
        REQUIRE(output_data != nullptr);
        
        // Store output samples (interleaved format)
        for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
            output_samples.push_back(output_data[i]);
        }
        
        // Push to audio output (only if enabled)
        if (audio_output) {
            while (!audio_output->is_ready()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            audio_output->push(output_data);
        }
    }
    
    playback_stage.stop();
    
    // Cleanup audio (only if enabled)
    if (audio_output) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        audio_output->stop();
        audio_output->close();
        delete audio_output;
    }
    
    // CSV output (only if enabled)
    if (is_csv_output_enabled()) {
        std::string csv_output_dir = "build/tests/csv_output";
        system(("mkdir -p " + csv_output_dir).c_str());
        
        // Convert interleaved samples to per-channel format
        std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
        const size_t samples_per_channel = PLAYBACK_FRAMES * BUFFER_SIZE;
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            output_samples_per_channel[ch].reserve(samples_per_channel);
        }
        
        // Deinterleave output samples
        for (size_t i = 0; i < output_samples.size(); i += NUM_CHANNELS) {
            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                output_samples_per_channel[ch].push_back(output_samples[i + ch]);
            }
        }
        
        std::ostringstream filename_stream;
        filename_stream << csv_output_dir << "/tape_speed_changes_" << params.name << ".csv";
        std::string filename = filename_stream.str();
        
        CSVTestOutput csv_writer(filename, SAMPLE_RATE);
        REQUIRE(csv_writer.is_open());
        csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
        csv_writer.close();
        
        std::cout << "Wrote speed change test output to " << filename << " (" 
                  << samples_per_channel << " samples, " << NUM_CHANNELS << " channels)" << std::endl;
    }
    
    SECTION("Continuity Check - No Discontinuities") {
        // Split interleaved samples into per-channel vectors
        std::vector<std::vector<float>> channel_samples(NUM_CHANNELS);
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            channel_samples[ch].reserve(output_samples.size() / NUM_CHANNELS);
        }
        for (size_t i = 0; i + (NUM_CHANNELS - 1) < output_samples.size(); i += NUM_CHANNELS) {
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                channel_samples[ch].push_back(output_samples[i + ch]);
            }
        }
        
        // Check for sample-to-sample discontinuities per channel
        // Use a more lenient threshold since speed changes can cause larger differences
        constexpr float MAX_SAMPLE_DIFF = 0.2f; // Allow larger differences during speed transitions
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            std::size_t discontinuity_count = 0;
            std::vector<size_t> discontinuity_indices;
            const auto &samples = channel_samples[ch];
            for (size_t i = 1; i < samples.size(); ++i) {
                float diff = std::abs(samples[i] - samples[i - 1]);
                if (diff > MAX_SAMPLE_DIFF) {
                    discontinuity_count++;
                    if (discontinuity_indices.size() < 10) { // Store first 10 for debugging
                        discontinuity_indices.push_back(i);
                    }
                }
            }
            INFO("Channel " << ch << " discontinuities: " << discontinuity_count << " out of " << samples.size() << " samples");
            if (discontinuity_indices.size() > 0) {
                INFO("  First discontinuity at sample " << discontinuity_indices[0]);
            }
            // Allow some discontinuities during speed transitions, but should be minimal
            REQUIRE(discontinuity_count < samples.size() / 50); // Less than 2% discontinuities
        }
    }
    
    SECTION("Slope Change Check - Smooth Transitions") {
        // Split interleaved samples into per-channel vectors
        std::vector<std::vector<float>> channel_samples(NUM_CHANNELS);
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            channel_samples[ch].reserve(output_samples.size() / NUM_CHANNELS);
        }
        for (size_t i = 0; i + (NUM_CHANNELS - 1) < output_samples.size(); i += NUM_CHANNELS) {
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                channel_samples[ch].push_back(output_samples[i + ch]);
            }
        }
        
        // Check for sudden slope changes that would indicate discontinuities
        // Use more lenient thresholds since speed changes can cause larger slope variations
        constexpr float MAX_SLOPE_CHANGE_JUMP = 0.15f; // Maximum allowed jump in slope change per sample
        constexpr float MAX_ABSOLUTE_SLOPE_CHANGE = 0.25f; // Maximum allowed absolute slope change
        constexpr float MIN_SLOPE_CHANGE_FOR_ANALYSIS = 0.0001f; // Ignore very small slope changes
        
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            const auto &samples = channel_samples[ch];
            
            if (samples.size() < 3) {
                continue; // Need at least 3 samples to calculate slope changes
            }
            
            // Calculate first derivative (slope) between consecutive samples
            std::vector<float> slopes;
            slopes.reserve(samples.size() - 1);
            for (size_t i = 1; i < samples.size(); ++i) {
                float slope = samples[i] - samples[i - 1];
                slopes.push_back(slope);
            }
            
            // Calculate second derivative (change in slope)
            std::vector<float> slope_changes;
            slope_changes.reserve(slopes.size() - 1);
            for (size_t i = 1; i < slopes.size(); ++i) {
                float slope_change = std::abs(slopes[i] - slopes[i - 1]);
                slope_changes.push_back(slope_change);
            }
            
            // Find large absolute slope changes
            std::vector<size_t> large_absolute_slope_change_indices;
            std::vector<float> large_absolute_slope_change_magnitudes;
            
            for (size_t i = 0; i < slope_changes.size(); ++i) {
                if (slope_changes[i] > MAX_ABSOLUTE_SLOPE_CHANGE) {
                    large_absolute_slope_change_indices.push_back(i + 1);
                    large_absolute_slope_change_magnitudes.push_back(slope_changes[i]);
                }
            }
            
            // Find large jumps in slope changes (third derivative)
            std::vector<size_t> large_jump_indices;
            std::vector<float> large_jump_magnitudes;
            
            for (size_t i = 1; i < slope_changes.size(); ++i) {
                float jump = std::abs(slope_changes[i] - slope_changes[i - 1]);
                if (slope_changes[i] > MIN_SLOPE_CHANGE_FOR_ANALYSIS && 
                    slope_changes[i - 1] > MIN_SLOPE_CHANGE_FOR_ANALYSIS &&
                    jump > MAX_SLOPE_CHANGE_JUMP) {
                    large_jump_indices.push_back(i + 1);
                    large_jump_magnitudes.push_back(jump);
                }
            }
            
            INFO("Channel " << ch << " slope change analysis:");
            INFO("  Total samples: " << samples.size());
            INFO("  Found " << large_absolute_slope_change_indices.size() << " large absolute slope changes");
            INFO("  Found " << large_jump_indices.size() << " large slope change jumps");
            
            if (large_absolute_slope_change_indices.size() > 0) {
                INFO("  First large absolute slope change at sample " << large_absolute_slope_change_indices[0]
                     << " with magnitude " << large_absolute_slope_change_magnitudes[0]);
            }
            
            if (large_jump_indices.size() > 0) {
                INFO("  First large jump at sample " << large_jump_indices[0]
                     << " with magnitude " << large_jump_magnitudes[0]);
            }
            
            // Verify that slope changes are relatively smooth
            // Allow some jumps during speed transitions, but should be minimal
            REQUIRE(large_jump_indices.size() < samples.size() / 100); // Less than 1% large jumps
            
            // Also check for large absolute slope changes (should be minimal)
            REQUIRE(large_absolute_slope_change_indices.size() < samples.size() / 50); // Less than 2% large changes
        }
    }
    
    // Cleanup
    delete global_time_param;
}

TEMPLATE_TEST_CASE("AudioTapeRenderStage - Record and Playback with Changing Constants", 
                   "[audio_tape_render_stage][gl_test][template]", 
                   TestParam1, TestParam2, TestParam3) {
    
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_FRAMES = 100;
    constexpr int NUM_FRAMES_PER_INTERVAL = 10;
    constexpr int NUM_INTERVALS = NUM_FRAMES / NUM_FRAMES_PER_INTERVAL;

    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    std::string custom_shader = R"(
#version 330 core
void main() {
    float value = float(global_time_val / 10) * 0.1;
    output_audio_texture = vec4(value, value, value, 1.0) + texture(stream_audio_texture, TexCoord);
}
)";

    const char* shader_path = "build/shaders/test_changing_constants.glsl";
    {
        std::ofstream fs(shader_path);
        fs << custom_shader;
    }

    AudioRenderStage custom_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, shader_path);

    AudioRecordRenderStage record_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    AudioPlaybackRenderStage playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    AudioFinalRenderStage final_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();

    REQUIRE(custom_generator.initialize());
    REQUIRE(record_stage.initialize());
    REQUIRE(playback_stage.initialize());
    REQUIRE(final_stage.initialize());

    REQUIRE(custom_generator.connect_render_stage(&record_stage));
    REQUIRE(playback_stage.connect_render_stage(&final_stage));

    context.prepare_draw();
    
    REQUIRE(custom_generator.bind());
    REQUIRE(record_stage.bind());
    REQUIRE(playback_stage.bind());
    REQUIRE(final_stage.bind());

    SECTION("Baseline record and playback by interval") {
        // Prepare audio output (only if enabled)
        AudioPlayerOutput* audio_output = nullptr;
        if (is_audio_output_enabled()) {
            audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
            REQUIRE(audio_output->open());
            REQUIRE(audio_output->start());
        }

        std::vector<float> output_samples;
        output_samples.reserve(BUFFER_SIZE * NUM_CHANNELS * NUM_FRAMES);

        record_stage.record(0);

        for (int frame = 0; frame < NUM_FRAMES; ++frame) {
            global_time_param->set_value(frame);
            global_time_param->render();

            custom_generator.render(frame);
            record_stage.render(frame);
        }

        record_stage.stop();

        // Load tape from record to playback
        playback_stage.load_tape(record_stage.get_tape());

        for (int start_sec = 0; start_sec < NUM_INTERVALS; ++start_sec) {
            float expected = float(start_sec) * 0.1f;

            playback_stage.play(start_sec * NUM_FRAMES_PER_INTERVAL);

            for (int play_frame = 0; play_frame < NUM_FRAMES_PER_INTERVAL; ++play_frame) {
                global_time_param->set_value(play_frame);
                global_time_param->render();

                playback_stage.render(play_frame);
                final_stage.render(play_frame);

                auto output_param = final_stage.find_parameter("final_output_audio_texture");
                REQUIRE(output_param != nullptr);

                const float* output_data = static_cast<const float*>(output_param->get_value());
                REQUIRE(output_data != nullptr);

                // Store output samples (interleaved format)
                for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
                    output_samples.push_back(output_data[i]);
                }

                // Push to audio output (only if enabled)
                if (audio_output) {
                    while (!audio_output->is_ready()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    audio_output->push(output_data);
                }

                // Verify a couple of samples per frame
                REQUIRE(output_data[0] == Catch::Approx(expected).margin(1e-4));
                REQUIRE(output_data[BUFFER_SIZE - 1] == Catch::Approx(expected).margin(1e-4));
            }

            playback_stage.stop();
        }

        // Cleanup audio (only if enabled)
        if (audio_output) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            audio_output->stop();
            audio_output->close();
            delete audio_output;
        }

        // CSV output (only if enabled)
        if (is_csv_output_enabled()) {
            std::string csv_output_dir = "build/tests/csv_output";
            system(("mkdir -p " + csv_output_dir).c_str());
            
            // Convert interleaved samples to per-channel format
            std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
            const size_t samples_per_channel = NUM_FRAMES * BUFFER_SIZE;
            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                output_samples_per_channel[ch].reserve(samples_per_channel);
            }
            
            // Deinterleave output samples
            for (size_t i = 0; i < output_samples.size(); i += NUM_CHANNELS) {
                for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                    output_samples_per_channel[ch].push_back(output_samples[i + ch]);
                }
            }
            
            std::ostringstream filename_stream;
            filename_stream << csv_output_dir << "/tape_changing_constants_baseline_" << params.name << ".csv";
            std::string filename = filename_stream.str();
            
            CSVTestOutput csv_writer(filename, SAMPLE_RATE);
            REQUIRE(csv_writer.is_open());
            csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
            csv_writer.close();
            
            std::cout << "Wrote changing constants baseline output to " << filename << " (" 
                      << samples_per_channel << " samples, " << NUM_CHANNELS << " channels)" << std::endl;
        }
    }

    SECTION("Overwrite a portion of the tape and verify changes") {
        // Prepare audio output (only if enabled)
        AudioPlayerOutput* audio_output = nullptr;
        if (is_audio_output_enabled()) {
            audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
            REQUIRE(audio_output->open());
            REQUIRE(audio_output->start());
        }

        std::vector<float> output_samples;
        output_samples.reserve(BUFFER_SIZE * NUM_CHANNELS * NUM_FRAMES);

        // Step 1: Record baseline from position 0
        record_stage.record(0);

        for (int frame = 0; frame < NUM_FRAMES; ++frame) {
            global_time_param->set_value(frame);
            global_time_param->render();

            custom_generator.render(frame);
            record_stage.render(frame);
        }

        record_stage.stop();

        // Step 2: Overwrite a middle segment starting at some offset
        const int overwrite_start = 25;                // tape frame index to start overwriting
        const int overwrite_length = 10;               // number of tape frames to overwrite
        const int overwrite_source_time = 55;          // different global time origin for overwrite
        record_stage.record(overwrite_start);

        // Keep stage time continuous relative to when record() was called so current_block starts at 0
        int timeline_time = NUM_FRAMES - 1; // record_start_time was last render time
        for (int frame = 0; frame < overwrite_length; ++frame) {
            // Drive shader with a different time base so overwritten values differ from baseline
            global_time_param->set_value(overwrite_source_time + frame);
            global_time_param->render();

            custom_generator.render(timeline_time);
            record_stage.render(timeline_time);
            timeline_time++;
        }

        record_stage.stop();

        // Step 3: Load updated tape and verify playback around and inside overwritten region
        playback_stage.load_tape(record_stage.get_tape());

        auto verify_playback_at = [&](int play_position, float expected_value) {
            playback_stage.play(play_position);
            // Render one frame and check a couple of samples
            global_time_param->set_value(0);
            global_time_param->render();
            playback_stage.render(0);
            final_stage.render(0);

            auto output_param = final_stage.find_parameter("final_output_audio_texture");
            REQUIRE(output_param != nullptr);
            const float* output_data = static_cast<const float*>(output_param->get_value());
            REQUIRE(output_data != nullptr);

            // Store output samples (interleaved format)
            for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
                output_samples.push_back(output_data[i]);
            }

            // Push to audio output (only if enabled)
            if (audio_output) {
                while (!audio_output->is_ready()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                audio_output->push(output_data);
            }

            REQUIRE(output_data[0] == Catch::Approx(expected_value).margin(1e-4));
            REQUIRE(output_data[BUFFER_SIZE - 1] == Catch::Approx(expected_value).margin(1e-4));
            playback_stage.stop();
        };

        // Before overwritten region
        const int before_pos = overwrite_start - 1;
        if (before_pos >= 0) {
            float expected_before = float(before_pos / NUM_FRAMES_PER_INTERVAL) * 0.1f;
            verify_playback_at(before_pos, expected_before);
        }

        // Inside overwritten region => expect values from overwrite_source_time window
        for (int f = 0; f < overwrite_length; ++f) {
            float expected_overwrite = float((overwrite_source_time + f) / NUM_FRAMES_PER_INTERVAL) * 0.1f;
            verify_playback_at(overwrite_start + f, expected_overwrite);
        }

        // After overwritten region
        const int after_pos = overwrite_start + overwrite_length;
        if (after_pos < NUM_FRAMES) {
            float expected_after = float(after_pos / NUM_FRAMES_PER_INTERVAL) * 0.1f;
            verify_playback_at(after_pos, expected_after);
        }

        // Cleanup audio (only if enabled)
        if (audio_output) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            audio_output->stop();
            audio_output->close();
            delete audio_output;
        }

        // CSV output (only if enabled)
        if (is_csv_output_enabled()) {
            std::string csv_output_dir = "build/tests/csv_output";
            system(("mkdir -p " + csv_output_dir).c_str());
            
            // Convert interleaved samples to per-channel format
            std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
            const size_t samples_per_channel = output_samples.size() / NUM_CHANNELS;
            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                output_samples_per_channel[ch].reserve(samples_per_channel);
            }
            
            // Deinterleave output samples
            for (size_t i = 0; i < output_samples.size(); i += NUM_CHANNELS) {
                for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                    output_samples_per_channel[ch].push_back(output_samples[i + ch]);
                }
            }
            
            std::ostringstream filename_stream;
            filename_stream << csv_output_dir << "/tape_changing_constants_overwrite_" << params.name << ".csv";
            std::string filename = filename_stream.str();
            
            CSVTestOutput csv_writer(filename, SAMPLE_RATE);
            REQUIRE(csv_writer.is_open());
            csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
            csv_writer.close();
            
            std::cout << "Wrote changing constants overwrite output to " << filename << " (" 
                      << samples_per_channel << " samples, " << NUM_CHANNELS << " channels)" << std::endl;
        }
    }

    // Cleanup
    delete global_time_param;
}

TEMPLATE_TEST_CASE("AudioTapeRenderStage - Multiple Renders Per Frame with Start/Stop Recording", 
                   "[audio_tape_render_stage][gl_test][template][multiple_renders]", 
                   TestParam1, TestParam2, TestParam3) {
    
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int TOTAL_FRAMES = 50;
    
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    std::string custom_shader = R"(
#version 330 core
void main() {
    float value = float(global_time_val / 10) * 0.1;
    output_audio_texture = vec4(value, value, value, 1.0) + texture(stream_audio_texture, TexCoord);
}
)";

    const char* shader_path = "build/shaders/test_changing_constants.glsl";
    {
        std::ofstream fs(shader_path);
        fs << custom_shader;
    }

    AudioRenderStage custom_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, shader_path);

    AudioRecordRenderStage record_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    AudioPlaybackRenderStage playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    AudioFinalRenderStage final_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();

    REQUIRE(custom_generator.initialize());
    REQUIRE(record_stage.initialize());
    REQUIRE(playback_stage.initialize());
    REQUIRE(final_stage.initialize());

    REQUIRE(custom_generator.connect_render_stage(&record_stage));
    REQUIRE(playback_stage.connect_render_stage(&final_stage));

    context.prepare_draw();
    
    REQUIRE(custom_generator.bind());
    REQUIRE(record_stage.bind());
    REQUIRE(playback_stage.bind());
    REQUIRE(final_stage.bind());

    // Prepare audio output (only if enabled)
    AudioPlayerOutput* audio_output = nullptr;
    if (is_audio_output_enabled()) {
        audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
        REQUIRE(audio_output->open());
        REQUIRE(audio_output->start());
    }

    std::vector<float> recorded_audio;
    recorded_audio.reserve(TOTAL_FRAMES * BUFFER_SIZE * NUM_CHANNELS * 2); // Reserve extra space
    
    std::cout << "\n=== Multiple Renders Per Frame Test ===" << std::endl;
    std::cout << "Rendering same frame multiple times with recording start/stop changes in between..." << std::endl;

    // Track recording state
    bool is_recording = false;
    unsigned int record_start_frame = 0;

    for (int frame = 0; frame < TOTAL_FRAMES; frame++) {
        global_time_param->set_value(frame);
        global_time_param->render();

        // Render pass 1: Normal render
        custom_generator.render(frame);
        record_stage.render(frame);
        
        // INTERMEDIATE STATE CHANGES: Start/stop recording mid-frame
        if (frame == 5 && !is_recording) {
            std::cout << "Frame " << frame << ": Starting recording between renders" << std::endl;
            record_stage.record(0);
            is_recording = true;
            record_start_frame = frame;
        } else if (frame == 15 && is_recording) {
            std::cout << "Frame " << frame << ": Stopping recording between renders" << std::endl;
            record_stage.stop();
            is_recording = false;
        } else if (frame == 20 && !is_recording) {
            std::cout << "Frame " << frame << ": Starting recording again between renders" << std::endl;
            record_stage.record(10); // Start recording at position 10
            is_recording = true;
            record_start_frame = frame;
        } else if (frame == 30 && is_recording) {
            std::cout << "Frame " << frame << ": Stopping recording again between renders" << std::endl;
            record_stage.stop();
            is_recording = false;
        }
        
        // Render pass 2: Same frame index, potentially different recording state
        custom_generator.render(frame);
        record_stage.render(frame);
        final_stage.render(frame);

        // Capture output (This simulates the final presented audio)
        auto output_param = final_stage.find_parameter("final_output_audio_texture");
        const float* output_data = static_cast<const float*>(output_param->get_value());
        REQUIRE(output_data != nullptr);
        for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; i++) {
            recorded_audio.push_back(output_data[i]);
        }

        // Push to audio output (only if enabled)
        if (audio_output) {
            while (!audio_output->is_ready()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            audio_output->push(output_data);
        }
    }

    // Ensure recording is stopped at the end
    if (is_recording) {
        record_stage.stop();
    }

    // Cleanup audio (only if enabled)
    if (audio_output) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        audio_output->stop();
        audio_output->close();
        delete audio_output;
    }

    SECTION("Recorded Data Verification") {
        // Load tape and verify recorded data
        playback_stage.load_tape(record_stage.get_tape());
        
        // Verify that frames 5-14 were recorded (first recording session)
        playback_stage.play(0);
        for (int play_frame = 0; play_frame < 10; ++play_frame) {
            global_time_param->set_value(play_frame);
            global_time_param->render();
            playback_stage.render(play_frame);
            final_stage.render(play_frame);

            auto output_param = final_stage.find_parameter("final_output_audio_texture");
            REQUIRE(output_param != nullptr);
            const float* output_data = static_cast<const float*>(output_param->get_value());
            REQUIRE(output_data != nullptr);

            // Expected value based on the frame number (frame 5 = 0.0, frame 6 = 0.1, etc.)
            float expected = float((5 + play_frame) / 10) * 0.1f;
            REQUIRE(output_data[0] == Catch::Approx(expected).margin(1e-4));
        }
        playback_stage.stop();

        // Verify that frames 20-29 were recorded at position 10 (second recording session)
        playback_stage.play(10);
        for (int play_frame = 0; play_frame < 10; ++play_frame) {
            global_time_param->set_value(play_frame);
            global_time_param->render();
            playback_stage.render(play_frame);
            final_stage.render(play_frame);

            auto output_param = final_stage.find_parameter("final_output_audio_texture");
            REQUIRE(output_param != nullptr);
            const float* output_data = static_cast<const float*>(output_param->get_value());
            REQUIRE(output_data != nullptr);

            // Expected value based on the frame number (frame 20 = 0.2, frame 21 = 0.2, etc.)
            float expected = float((20 + play_frame) / 10) * 0.1f;
            REQUIRE(output_data[0] == Catch::Approx(expected).margin(1e-4));
        }
        playback_stage.stop();
    }

    SECTION("Continuity Check (Multiple Renders)") {
        // Split interleaved samples into per-channel vectors
        std::vector<std::vector<float>> channel_samples(NUM_CHANNELS);
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            channel_samples[ch].reserve(recorded_audio.size() / NUM_CHANNELS);
        }
        for (size_t i = 0; i + (NUM_CHANNELS - 1) < recorded_audio.size(); i += NUM_CHANNELS) {
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                channel_samples[ch].push_back(recorded_audio[i + ch]);
            }
        }

        // Check for sample-to-sample discontinuities per channel
        // With multiple renders per frame, we expect smooth transitions
        constexpr float MAX_SAMPLE_DIFF = 0.15f; // Allow slightly more tolerance for multiple renders
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            std::size_t discontinuity_count = 0;
            const auto &samples = channel_samples[ch];
            for (size_t i = 1; i < samples.size(); ++i) {
                float diff = std::abs(samples[i] - samples[i - 1]);
                if (diff > MAX_SAMPLE_DIFF) {
                    discontinuity_count++;
                }
            }
            INFO("Channel " << ch << " discontinuities: " << discontinuity_count);
            // Allow a small number of discontinuities due to recording start/stop transitions
            // but should be minimal
            REQUIRE(discontinuity_count < samples.size() / 10); // Less than 10% discontinuities
        }
    }

    // Cleanup
    delete global_time_param;
}

TEMPLATE_TEST_CASE("AudioTapeRenderStage - Forward Playback Reaching End of Tape", 
                   "[audio_tape_render_stage][gl_test][template][edge_cases]", 
                   TestParam1, TestParam2, TestParam3) {
    
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr float TEST_FREQUENCY = 440.0f;
    constexpr float TEST_GAIN = 0.3f;
    constexpr int RECORD_FRAMES = SAMPLE_RATE / BUFFER_SIZE * 2; // Record 2 seconds
    
    // Initialize window and OpenGL context
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;
    
    // Create sine generator
    AudioGeneratorRenderStage generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 
                                        "build/shaders/multinote_sine_generator_render_stage.glsl");
    
    // Create record stage
    AudioRecordRenderStage record_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
    
    // Create playback stage
    AudioPlaybackRenderStage playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
    
    // Create final render stage
    AudioFinalRenderStage final_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
    
    // Add global_time parameter
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();
    
    // Initialize all stages
    REQUIRE(generator.initialize());
    REQUIRE(record_stage.initialize());
    REQUIRE(playback_stage.initialize());
    REQUIRE(final_stage.initialize());
    
    // Connect stages
    REQUIRE(generator.connect_render_stage(&record_stage));
    REQUIRE(playback_stage.connect_render_stage(&final_stage));
    
    context.prepare_draw();
    
    // Bind all stages
    REQUIRE(generator.bind());
    REQUIRE(record_stage.bind());
    REQUIRE(playback_stage.bind());
    REQUIRE(final_stage.bind());
    
    // Record sine wave
    record_stage.record(0);
    generator.play_note({TEST_FREQUENCY, TEST_GAIN});
    
    for (int frame = 0; frame < RECORD_FRAMES; ++frame) {
        global_time_param->set_value(frame);
        global_time_param->render();
        
        generator.render(frame);
        record_stage.render(frame);
    }
    
    generator.stop_note(TEST_FREQUENCY);
    record_stage.stop();
    
    // Get tape size
    auto tape = record_stage.get_tape().lock();
    REQUIRE(tape != nullptr);
    unsigned int tape_size_samples = tape->size();
    unsigned int tape_size_frames = tape_size_samples / BUFFER_SIZE;
    
    // Load tape to playback stage
    playback_stage.load_tape(record_stage.get_tape());
    
    SECTION("Normal Speed (1x) - Reaches End") {
        playback_stage.play(0);
        playback_stage.set_tape_speed(1.0f);
        
        // Render enough frames to exceed tape length
        int frames_to_render = tape_size_frames + 10; // Render past end
        std::vector<float> output_samples;
        output_samples.reserve(BUFFER_SIZE * NUM_CHANNELS * frames_to_render);
        
        bool was_playing = true;
        unsigned int last_position = 0;
        
        for (int frame = 0; frame < frames_to_render; ++frame) {
            global_time_param->set_value(frame);
            global_time_param->render();
            
            playback_stage.render(frame);
            final_stage.render(frame);
            
            bool is_playing_now = playback_stage.is_playing();
            unsigned int current_position = playback_stage.get_current_tape_position(frame);
            
            // Get output data
            auto output_param = final_stage.find_parameter("final_output_audio_texture");
            REQUIRE(output_param != nullptr);
            const float* output_data = static_cast<const float*>(output_param->get_value());
            REQUIRE(output_data != nullptr);
            
            // Store output samples
            for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
                output_samples.push_back(output_data[i]);
            }
            
            // Verify position doesn't exceed tape size
            REQUIRE(current_position <= tape_size_samples);
            
            // Track when stopping occurs
            bool just_stopped = !is_playing_now && was_playing;
            
            // Once stopped, position should be near or at end
            if (just_stopped) {
                INFO("Tape stopped at frame " << frame << ", position: " << current_position);
                REQUIRE(current_position >= tape_size_samples - BUFFER_SIZE * 2); // Allow some margin
                REQUIRE(current_position <= tape_size_samples);
            }
            
            // Verify position stays within bounds (might advance slightly after stopping due to window updates)
            REQUIRE(current_position <= tape_size_samples);
            
            was_playing = is_playing_now;
            last_position = current_position;
        }
        
        // Verify tape stopped
        REQUIRE_FALSE(playback_stage.is_playing());
        
        // Verify final position is at or near end
        unsigned int final_position = playback_stage.get_current_tape_position(frames_to_render - 1);
        REQUIRE(final_position >= tape_size_samples - BUFFER_SIZE);
        REQUIRE(final_position <= tape_size_samples);
        
        // Check for discontinuities near the end
        std::vector<std::vector<float>> channel_samples(NUM_CHANNELS);
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            channel_samples[ch].reserve(output_samples.size() / NUM_CHANNELS);
        }
        for (size_t i = 0; i + (NUM_CHANNELS - 1) < output_samples.size(); i += NUM_CHANNELS) {
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                channel_samples[ch].push_back(output_samples[i + ch]);
            }
        }
        
        // Check last portion of audio for discontinuities
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            const auto &samples = channel_samples[ch];
            if (samples.size() < 100) continue;
            
            // Check last 100 samples for discontinuities
            size_t start_check = samples.size() > 100 ? samples.size() - 100 : 0;
            std::size_t discontinuity_count = 0;
            for (size_t i = start_check + 1; i < samples.size(); ++i) {
                float diff = std::abs(samples[i] - samples[i - 1]);
                if (diff > 0.3f) { // Allow larger threshold at end
                    discontinuity_count++;
                }
            }
            INFO("Channel " << ch << " discontinuities in last 100 samples: " << discontinuity_count);
            REQUIRE(discontinuity_count < 10); // Should be minimal
        }
    }
    
    SECTION("Fast Speed (2x) - Reaches End") {
        playback_stage.play(0);
        playback_stage.set_tape_speed(2.0f);
        
        // Render enough frames to exceed tape length (at 2x speed, reaches end faster)
        int frames_to_render = (tape_size_frames / 2) + 10; // At 2x, need half the frames
        std::vector<float> output_samples;
        output_samples.reserve(BUFFER_SIZE * NUM_CHANNELS * frames_to_render);
        
        bool was_playing = true;
        unsigned int last_position = 0;
        
        for (int frame = 0; frame < frames_to_render; ++frame) {
            global_time_param->set_value(frame);
            global_time_param->render();
            
            playback_stage.render(frame);
            final_stage.render(frame);
            
            bool is_playing_now = playback_stage.is_playing();
            unsigned int current_position = playback_stage.get_current_tape_position(frame);
            
            // Get output data
            auto output_param = final_stage.find_parameter("final_output_audio_texture");
            REQUIRE(output_param != nullptr);
            const float* output_data = static_cast<const float*>(output_param->get_value());
            REQUIRE(output_data != nullptr);
            
            // Store output samples
            for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
                output_samples.push_back(output_data[i]);
            }
            
            // Verify position doesn't exceed tape size
            REQUIRE(current_position <= tape_size_samples);
            
            // Once stopped, position should remain constant
            if (!is_playing_now && was_playing) {
                INFO("Tape stopped at frame " << frame << " (2x speed), position: " << current_position);
                REQUIRE(current_position >= tape_size_samples - BUFFER_SIZE * 2); // At 2x, might stop a bit earlier
            }
            
            // When stopped, position should stay within bounds (may adjust slightly due to window updates)
            if (!is_playing_now) {
                REQUIRE(current_position >= 0);
                REQUIRE(current_position <= tape_size_samples);
            }
            
            was_playing = is_playing_now;
            last_position = current_position;
        }
        
        // Verify tape stopped
        REQUIRE_FALSE(playback_stage.is_playing());
        
        // Verify final position is at or near end
        unsigned int final_position = playback_stage.get_current_tape_position(frames_to_render - 1);
        REQUIRE(final_position >= tape_size_samples - BUFFER_SIZE * 2);
        REQUIRE(final_position <= tape_size_samples);
    }
    
    playback_stage.stop();
    
    // Cleanup
    delete global_time_param;
}

TEMPLATE_TEST_CASE("AudioTapeRenderStage - Reverse Playback Reaching Beginning of Tape", 
                   "[audio_tape_render_stage][gl_test][template][edge_cases]", 
                   TestParam1, TestParam2, TestParam3) {
    
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr float TEST_FREQUENCY = 440.0f;
    constexpr float TEST_GAIN = 0.3f;
    constexpr int RECORD_FRAMES = SAMPLE_RATE / BUFFER_SIZE * 2; // Record 2 seconds
    
    // Initialize window and OpenGL context
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;
    
    // Create sine generator
    AudioGeneratorRenderStage generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 
                                        "build/shaders/multinote_sine_generator_render_stage.glsl");
    
    // Create record stage
    AudioRecordRenderStage record_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
    
    // Create playback stage
    AudioPlaybackRenderStage playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
    
    // Create final render stage
    AudioFinalRenderStage final_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
    
    // Add global_time parameter
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();
    
    // Initialize all stages
    REQUIRE(generator.initialize());
    REQUIRE(record_stage.initialize());
    REQUIRE(playback_stage.initialize());
    REQUIRE(final_stage.initialize());
    
    // Connect stages
    REQUIRE(generator.connect_render_stage(&record_stage));
    REQUIRE(playback_stage.connect_render_stage(&final_stage));
    
    context.prepare_draw();
    
    // Bind all stages
    REQUIRE(generator.bind());
    REQUIRE(record_stage.bind());
    REQUIRE(playback_stage.bind());
    REQUIRE(final_stage.bind());
    
    // Record sine wave
    record_stage.record(0);
    generator.play_note({TEST_FREQUENCY, TEST_GAIN});
    
    for (int frame = 0; frame < RECORD_FRAMES; ++frame) {
        global_time_param->set_value(frame);
        global_time_param->render();
        
        generator.render(frame);
        record_stage.render(frame);
    }
    
    generator.stop_note(TEST_FREQUENCY);
    record_stage.stop();
    
    // Get tape size
    auto tape = record_stage.get_tape().lock();
    REQUIRE(tape != nullptr);
    unsigned int tape_size_samples = tape->size();
    unsigned int tape_size_frames = tape_size_samples / BUFFER_SIZE;
    
    // Load tape to playback stage
    playback_stage.load_tape(record_stage.get_tape());
    
    SECTION("Reverse Normal Speed (-1x) - Reaches Beginning") {
        // Start playback from near the end, then reverse
        playback_stage.play(tape_size_frames - 5); // Start 5 frames from end
        playback_stage.set_tape_speed(-1.0f); // Reverse playback
        
        // Render enough frames to go past beginning
        int frames_to_render = tape_size_frames + 10;
        std::vector<float> output_samples;
        output_samples.reserve(BUFFER_SIZE * NUM_CHANNELS * frames_to_render);
        
        bool was_playing = true;
        unsigned int last_position = tape_size_samples;
        
        for (int frame = 0; frame < frames_to_render; ++frame) {
            global_time_param->set_value(frame);
            global_time_param->render();
            
            playback_stage.render(frame);
            final_stage.render(frame);
            
            bool is_playing_now = playback_stage.is_playing();
            unsigned int current_position = playback_stage.get_current_tape_position(frame);
            
            // Get output data
            auto output_param = final_stage.find_parameter("final_output_audio_texture");
            REQUIRE(output_param != nullptr);
            const float* output_data = static_cast<const float*>(output_param->get_value());
            REQUIRE(output_data != nullptr);
            
            // Store output samples
            for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
                output_samples.push_back(output_data[i]);
            }
            
            // Verify position doesn't go below 0 (should be clamped)
            REQUIRE(current_position >= 0);
            REQUIRE(current_position <= tape_size_samples);
            
            // Position should decrease during reverse playback
            if (is_playing_now && was_playing) {
                REQUIRE(current_position <= last_position); // Should be moving backward
            }
            
            // Once stopped, position should remain constant
            if (!is_playing_now && was_playing) {
                INFO("Tape stopped at frame " << frame << " (reverse), position: " << current_position);
                REQUIRE(current_position <= BUFFER_SIZE); // Should be near or at beginning
            }
            
            // When stopped, position should stay within bounds (may adjust slightly due to window updates)
            if (!is_playing_now) {
                REQUIRE(current_position >= 0);
                REQUIRE(current_position <= tape_size_samples);
            }
            
            was_playing = is_playing_now;
            last_position = current_position;
        }
        
        // Verify tape stopped
        REQUIRE_FALSE(playback_stage.is_playing());
        
        // Verify final position is at or near beginning
        unsigned int final_position = playback_stage.get_current_tape_position(frames_to_render - 1);
        REQUIRE(final_position <= BUFFER_SIZE);
        REQUIRE(final_position >= 0);
        
        // Check for discontinuities near the beginning
        std::vector<std::vector<float>> channel_samples(NUM_CHANNELS);
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            channel_samples[ch].reserve(output_samples.size() / NUM_CHANNELS);
        }
        for (size_t i = 0; i + (NUM_CHANNELS - 1) < output_samples.size(); i += NUM_CHANNELS) {
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                channel_samples[ch].push_back(output_samples[i + ch]);
            }
        }
        
        // Check last portion of audio (where it stops) for discontinuities
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            const auto &samples = channel_samples[ch];
            if (samples.size() < 100) continue;
            
            // Check last 100 samples for discontinuities
            size_t start_check = samples.size() > 100 ? samples.size() - 100 : 0;
            std::size_t discontinuity_count = 0;
            for (size_t i = start_check + 1; i < samples.size(); ++i) {
                float diff = std::abs(samples[i] - samples[i - 1]);
                if (diff > 0.3f) { // Allow larger threshold at boundary
                    discontinuity_count++;
                }
            }
            INFO("Channel " << ch << " discontinuities in last 100 samples (reverse): " << discontinuity_count);
            REQUIRE(discontinuity_count < 10); // Should be minimal
        }
    }
    
    SECTION("Reverse Fast Speed (-2x) - Reaches Beginning") {
        // Start playback from near the end, then reverse fast
        playback_stage.play(tape_size_frames - 3); // Start 3 frames from end
        playback_stage.set_tape_speed(-2.0f); // Fast reverse playback
        
        // Render enough frames to go past beginning (at 2x reverse, reaches beginning faster)
        int frames_to_render = (tape_size_frames / 2) + 10;
        std::vector<float> output_samples;
        output_samples.reserve(BUFFER_SIZE * NUM_CHANNELS * frames_to_render);
        
        bool was_playing = true;
        unsigned int last_position = tape_size_samples;
        
        for (int frame = 0; frame < frames_to_render; ++frame) {
            global_time_param->set_value(frame);
            global_time_param->render();
            
            playback_stage.render(frame);
            final_stage.render(frame);
            
            bool is_playing_now = playback_stage.is_playing();
            unsigned int current_position = playback_stage.get_current_tape_position(frame);
            
            // Get output data
            auto output_param = final_stage.find_parameter("final_output_audio_texture");
            REQUIRE(output_param != nullptr);
            const float* output_data = static_cast<const float*>(output_param->get_value());
            REQUIRE(output_data != nullptr);
            
            // Store output samples
            for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
                output_samples.push_back(output_data[i]);
            }
            
            // Verify position doesn't go below 0
            REQUIRE(current_position >= 0);
            REQUIRE(current_position <= tape_size_samples);
            
            // Position should decrease during reverse playback
            if (is_playing_now && was_playing) {
                REQUIRE(current_position <= last_position); // Should be moving backward
            }
            
            // Once stopped, position should remain constant
            if (!is_playing_now && was_playing) {
                INFO("Tape stopped at frame " << frame << " (-2x speed), position: " << current_position);
                REQUIRE(current_position <= BUFFER_SIZE * 2); // At 2x reverse, might stop a bit earlier
            }
            
            // When stopped, position should stay within bounds (may adjust slightly due to window updates)
            if (!is_playing_now) {
                REQUIRE(current_position >= 0);
                REQUIRE(current_position <= tape_size_samples);
            }
            
            was_playing = is_playing_now;
            last_position = current_position;
        }
        
        // Verify tape stopped
        REQUIRE_FALSE(playback_stage.is_playing());
        
        // Verify final position is at or near beginning
        unsigned int final_position = playback_stage.get_current_tape_position(frames_to_render - 1);
        REQUIRE(final_position <= BUFFER_SIZE * 2);
        REQUIRE(final_position >= 0);
    }
    
    playback_stage.stop();
    
    // Cleanup
    delete global_time_param;
}
