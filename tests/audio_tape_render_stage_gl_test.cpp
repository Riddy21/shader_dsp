#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"

#include "audio_render_stage/audio_tape_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"
#include "audio_output/audio_player_output.h"
#include "audio_render_stage/audio_generator_render_stage.h"

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
    generator.play_note(TEST_FREQUENCY, TEST_GAIN);

    // Prepare audio output
    AudioPlayerOutput audio_output(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
    REQUIRE(audio_output.open());
    REQUIRE(audio_output.start());

    std::vector<float> input_samples;
    input_samples.reserve(BUFFER_SIZE * NUM_FRAMES);

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

        for (int i = 0; i < BUFFER_SIZE; ++i) {
            input_samples.push_back(input_data[i]);
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
    output_samples.reserve(BUFFER_SIZE * NUM_FRAMES);

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

        // Store for verification
        for (int i = 0; i < BUFFER_SIZE; ++i) {
            output_samples.push_back(before_final_data[i]);
        }

        // Push to audio output
        while (!audio_output.is_ready()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        audio_output.push(output_data);
    }

    // Stop playback
    playback_stage.stop();

    // Cleanup audio
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    audio_output.stop();
    audio_output.close();

    //SECTION("Export CSV") {
    //    // Build a base filename that encodes the test parameter combination
    //    std::string csv_base = std::string("audio_simple_record_playback_") + params.name;

    //    // 1) Write playback output samples (interleaved by channel)
    //    {
    //        std::ofstream csv_out(csv_base + std::string("_output.csv"), std::ios::out | std::ios::trunc);
    //        if (csv_out.is_open()) {
    //            // Header
    //            csv_out << "sample_index";
    //            csv_out << ",input";
    //            csv_out << ",output";
    //            csv_out << '\n';

    //            const std::size_t total_samples = input_samples.size();
    //            for (std::size_t i = 0; i < total_samples; ++i) {
    //                csv_out << i;
    //                csv_out << ',' << input_samples[i];
    //                csv_out << ',' << output_samples[i];
    //                csv_out << ',';
    //            }
    //            csv_out.close();
    //        }
    //    }
    //}

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

                // Verify a couple of samples per frame
                REQUIRE(output_data[0] == Catch::Approx(expected).margin(1e-6));
                REQUIRE(output_data[BUFFER_SIZE - 1] == Catch::Approx(expected).margin(1e-6));
            }

            playback_stage.stop();
        }
    }

    SECTION("Overwrite a portion of the tape and verify changes") {
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

            REQUIRE(output_data[0] == Catch::Approx(expected_value).margin(1e-6));
            REQUIRE(output_data[BUFFER_SIZE - 1] == Catch::Approx(expected_value).margin(1e-6));
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
    }

    // Cleanup
    delete global_time_param;
}
