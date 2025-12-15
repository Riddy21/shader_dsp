#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"
#include "framework/test_main.h"

#include "audio_render_stage/audio_tape_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_output/audio_player_output.h"
#include "framework/csv_test_output.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <vector>
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

TEMPLATE_TEST_CASE("AudioRecordRenderStage - Record from Different Tape Positions", 
                   "[audio_record_render_stage][gl_test][template]", 
                   TestParam1, TestParam2, TestParam3) {
    
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_FRAMES = 20;

    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    std::string custom_shader = R"(
#version 330 core
void main() {
    float value = float(global_time_val) * 0.01;
    output_audio_texture = vec4(value, value, value, 1.0) + texture(stream_audio_texture, TexCoord);
}
)";

    const char* shader_path = "build/shaders/test_record_constants.glsl";
    {
        std::ofstream fs(shader_path);
        if (fs.is_open()) {
            fs.write(custom_shader.c_str(), custom_shader.size());
        }
    }

    AudioRenderStage custom_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, shader_path);
    AudioRecordRenderStage record_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();

    REQUIRE(custom_generator.initialize());
    REQUIRE(record_stage.initialize());

    REQUIRE(custom_generator.connect_render_stage(&record_stage));

    context.prepare_draw();
    
    REQUIRE(custom_generator.bind());
    REQUIRE(record_stage.bind());

    SECTION("Record starting at position 0") {
        // Prepare audio output (only if enabled)
        AudioPlayerOutput* audio_output = nullptr;
        if (is_audio_output_enabled()) {
            audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
            REQUIRE(audio_output->open());
            REQUIRE(audio_output->start());
        }

        record_stage.record(0);

        for (int frame = 0; frame < NUM_FRAMES; ++frame) {
            global_time_param->set_value(frame);
            global_time_param->render();

            custom_generator.render(frame);
            record_stage.render(frame);

            // Get generator output for audio playback
            auto generator_param = custom_generator.find_parameter("output_audio_texture");
            if (generator_param && audio_output) {
                const float* generator_data = static_cast<const float*>(generator_param->get_value());
                if (generator_data) {
                    // Convert channel-major to interleaved for audio output
                    std::vector<float> interleaved(BUFFER_SIZE * NUM_CHANNELS);
                    for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                        const float* channel_data = generator_data + (ch * BUFFER_SIZE);
                        for (int i = 0; i < BUFFER_SIZE; ++i) {
                            interleaved[i * NUM_CHANNELS + ch] = channel_data[i];
                        }
                    }
                    while (!audio_output->is_ready()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    audio_output->push(interleaved.data());
                }
            }
        }

        record_stage.stop();

        // Cleanup audio (only if enabled)
        if (audio_output) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            audio_output->stop();
            audio_output->close();
            delete audio_output;
        }

        // Verify recorded data
        AudioTape* tape = record_stage.get_tape_new();
        REQUIRE(tape != nullptr);
        REQUIRE(tape->size() >= NUM_FRAMES * BUFFER_SIZE);

        // Check that frames 0-19 were recorded correctly
        for (int frame = 0; frame < NUM_FRAMES; ++frame) {
            float expected_value = float(frame) * 0.01f;
            unsigned int sample_offset = frame * BUFFER_SIZE;
            
            // Get playback data for this frame (one frame = BUFFER_SIZE samples per channel)
            auto playback_data = tape->playback(BUFFER_SIZE, sample_offset, false);
            REQUIRE(playback_data.size() == BUFFER_SIZE * NUM_CHANNELS);

            // Verify all samples in this frame have the expected value
            for (int i = 0; i < BUFFER_SIZE; ++i) {
                for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                    unsigned int idx = ch * BUFFER_SIZE + i;
                    REQUIRE(playback_data[idx] == Catch::Approx(expected_value).margin(1e-5));
                }
            }
        }

        // CSV output (only if enabled)
        if (is_csv_output_enabled()) {
            std::string csv_output_dir = "build/tests/csv_output";
            system(("mkdir -p " + csv_output_dir).c_str());
            
            std::vector<std::vector<float>> recorded_samples_per_channel(NUM_CHANNELS);
            const size_t samples_per_channel = NUM_FRAMES * BUFFER_SIZE;
            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                recorded_samples_per_channel[ch].reserve(samples_per_channel);
            }
            
            // Read all recorded data from tape
            for (int frame = 0; frame < NUM_FRAMES; ++frame) {
                unsigned int sample_offset = frame * BUFFER_SIZE;
                auto playback_data = tape->playback(BUFFER_SIZE, sample_offset, false);
                REQUIRE(playback_data.size() == BUFFER_SIZE * NUM_CHANNELS);
                
                // Convert from channel-major to per-channel vectors
                for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                    for (int i = 0; i < BUFFER_SIZE; ++i) {
                        unsigned int idx = ch * BUFFER_SIZE + i;
                        recorded_samples_per_channel[ch].push_back(playback_data[idx]);
                    }
                }
            }
            
            std::ostringstream filename_stream;
            filename_stream << csv_output_dir << "/record_render_stage_position_0_" << params.name << ".csv";
            std::string filename = filename_stream.str();
            
            CSVTestOutput csv_writer(filename, SAMPLE_RATE);
            REQUIRE(csv_writer.is_open());
            csv_writer.write_channels(recorded_samples_per_channel, SAMPLE_RATE);
            csv_writer.close();
            
            std::cout << "Wrote recorded samples to " << filename << " (" 
                      << recorded_samples_per_channel[0].size() << " samples, " << NUM_CHANNELS << " channels)" << std::endl;
        }
    }

    SECTION("Record starting at position 10") {
        record_stage.record(10);

        for (int frame = 0; frame < NUM_FRAMES; ++frame) {
            global_time_param->set_value(frame);
            global_time_param->render();

            custom_generator.render(frame);
            record_stage.render(frame);
        }

        record_stage.stop();

        // Verify recorded data
        AudioTape* tape = record_stage.get_tape_new();
        REQUIRE(tape != nullptr);
        REQUIRE(tape->size() >= (10 + NUM_FRAMES) * BUFFER_SIZE);

        // Check that frames 0-19 were recorded at position 10-29
        for (int frame = 0; frame < NUM_FRAMES; ++frame) {
            float expected_value = float(frame) * 0.01f;
            unsigned int sample_offset = (10 + frame) * BUFFER_SIZE;
            
            // Get playback data for this frame (one frame = BUFFER_SIZE samples per channel)
            auto playback_data = tape->playback(BUFFER_SIZE, sample_offset, false);
            REQUIRE(playback_data.size() == BUFFER_SIZE * NUM_CHANNELS);

            // Verify all samples in this frame have the expected value
            for (int i = 0; i < BUFFER_SIZE; ++i) {
                for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                    unsigned int idx = ch * BUFFER_SIZE + i;
                    REQUIRE(playback_data[idx] == Catch::Approx(expected_value).margin(1e-5));
                }
            }
        }

        // Verify that positions 0-9 are zero (not recorded)
        for (int frame = 0; frame < 10; ++frame) {
            unsigned int sample_offset = frame * BUFFER_SIZE;
            auto playback_data = tape->playback(BUFFER_SIZE, sample_offset, false);
            REQUIRE(playback_data.size() == BUFFER_SIZE * NUM_CHANNELS);

            for (int i = 0; i < BUFFER_SIZE; ++i) {
                for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                    unsigned int idx = ch * BUFFER_SIZE + i;
                    REQUIRE(playback_data[idx] == Catch::Approx(0.0f).margin(1e-5));
                }
            }
        }
    }

    SECTION("Record at position 0, then change to position 15 mid-recording") {
        // Start recording at position 0
        record_stage.record(0);

        // Record frames 0-9 at position 0-9
        for (int frame = 0; frame < 10; ++frame) {
            global_time_param->set_value(frame);
            global_time_param->render();

            custom_generator.render(frame);
            record_stage.render(frame);
        }

        // Change record position to 15
        record_stage.stop();
        record_stage.record(15);

        // Record frames 10-19 at position 15-24
        for (int frame = 10; frame < NUM_FRAMES; ++frame) {
            global_time_param->set_value(frame);
            global_time_param->render();

            custom_generator.render(frame);
            record_stage.render(frame);
        }

        record_stage.stop();

        // Verify recorded data
        AudioTape* tape = record_stage.get_tape_new();
        REQUIRE(tape != nullptr);

        // Check first recording session (frames 0-9 at positions 0-9)
        for (int frame = 0; frame < 10; ++frame) {
            float expected_value = float(frame) * 0.01f;
            unsigned int sample_offset = frame * BUFFER_SIZE;
            
            auto playback_data = tape->playback(BUFFER_SIZE, sample_offset, false);
            REQUIRE(playback_data.size() == BUFFER_SIZE * NUM_CHANNELS);

            for (int i = 0; i < BUFFER_SIZE; ++i) {
                for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                    unsigned int idx = ch * BUFFER_SIZE + i;
                    REQUIRE(playback_data[idx] == Catch::Approx(expected_value).margin(1e-5));
                }
            }
        }

        // Check second recording session (frames 10-19 at positions 16-25)
        // Note: When record() is called after frame 9, m_record_start_time = 9
        // So frame 10 has current_block = 10 - 9 = 1, record_time = 1 + 15 = 16
        for (int frame = 10; frame < NUM_FRAMES; ++frame) {
            float expected_value = float(frame) * 0.01f;
            unsigned int tape_position = 16 + (frame - 10); // Starts at 16, not 15
            unsigned int sample_offset = tape_position * BUFFER_SIZE;
            
            auto playback_data = tape->playback(BUFFER_SIZE, sample_offset, false);
            REQUIRE(playback_data.size() == BUFFER_SIZE * NUM_CHANNELS);

            for (int i = 0; i < BUFFER_SIZE; ++i) {
                for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                    unsigned int idx = ch * BUFFER_SIZE + i;
                    REQUIRE(playback_data[idx] == Catch::Approx(expected_value).margin(1e-5));
                }
            }
        }

        // Verify gap positions 10-15 are zero (between first session ending at 9 and second starting at 16)
        for (int frame = 10; frame < 16; ++frame) {
            unsigned int sample_offset = frame * BUFFER_SIZE;
            auto playback_data = tape->playback(BUFFER_SIZE, sample_offset, false);
            REQUIRE(playback_data.size() == BUFFER_SIZE * NUM_CHANNELS);

            for (int i = 0; i < BUFFER_SIZE; ++i) {
                for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                    unsigned int idx = ch * BUFFER_SIZE + i;
                    REQUIRE(playback_data[idx] == Catch::Approx(0.0f).margin(1e-5));
                }
            }
        }
    }

    // Cleanup
    delete global_time_param;
}

TEMPLATE_TEST_CASE("AudioRecordRenderStage - Change Time Mid-Recording", 
                   "[audio_record_render_stage][gl_test][template]", 
                   TestParam1, TestParam2, TestParam3) {
    
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_FRAMES = 20;

    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    std::string custom_shader = R"(
#version 330 core
void main() {
    float value = float(global_time_val) * 0.01;
    output_audio_texture = vec4(value, value, value, 1.0) + texture(stream_audio_texture, TexCoord);
}
)";

    const char* shader_path = "build/shaders/test_record_constants.glsl";
    {
        std::ofstream fs(shader_path);
        if (fs.is_open()) {
            fs.write(custom_shader.c_str(), custom_shader.size());
        }
    }

    AudioRenderStage custom_generator(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, shader_path);
    AudioRecordRenderStage record_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();

    REQUIRE(custom_generator.initialize());
    REQUIRE(record_stage.initialize());

    REQUIRE(custom_generator.connect_render_stage(&record_stage));

    context.prepare_draw();
    
    REQUIRE(custom_generator.bind());
    REQUIRE(record_stage.bind());

    SECTION("Change global_time mid-recording") {
        record_stage.record(0);

        // Record frames 0-9 with normal time progression
        for (int frame = 0; frame < 10; ++frame) {
            global_time_param->set_value(frame);
            global_time_param->render();

            custom_generator.render(frame);
            record_stage.render(frame);
        }

        // Jump time forward by 10 frames
        // This simulates a time jump in the system
        for (int frame = 10; frame < NUM_FRAMES; ++frame) {
            // Use frame + 10 to simulate time jump
            global_time_param->set_value(frame + 10);
            global_time_param->render();

            custom_generator.render(frame);
            record_stage.render(frame);
        }

        record_stage.stop();

        // Verify recorded data
        AudioTape* tape = record_stage.get_tape_new();
        REQUIRE(tape != nullptr);

        // Check first part (frames 0-9, recorded with time 0-9)
        for (int frame = 0; frame < 10; ++frame) {
            float expected_value = float(frame) * 0.01f;
            unsigned int sample_offset = frame * BUFFER_SIZE;
            
            auto playback_data = tape->playback(BUFFER_SIZE, sample_offset, false);
            REQUIRE(playback_data.size() == BUFFER_SIZE * NUM_CHANNELS);

            for (int i = 0; i < BUFFER_SIZE; ++i) {
                for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                    unsigned int idx = ch * BUFFER_SIZE + i;
                    REQUIRE(playback_data[idx] == Catch::Approx(expected_value).margin(1e-5));
                }
            }
        }

        // Check second part (frames 10-19, recorded with time 20-29)
        // The tape position should still be 10-19, but the values should reflect time 20-29
        for (int frame = 10; frame < NUM_FRAMES; ++frame) {
            float expected_value = float(frame + 10) * 0.01f; // Time was frame + 10
            unsigned int sample_offset = frame * BUFFER_SIZE;
            
            auto playback_data = tape->playback(BUFFER_SIZE, sample_offset, false);
            REQUIRE(playback_data.size() == BUFFER_SIZE * NUM_CHANNELS);

            for (int i = 0; i < BUFFER_SIZE; ++i) {
                for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                    unsigned int idx = ch * BUFFER_SIZE + i;
                    REQUIRE(playback_data[idx] == Catch::Approx(expected_value).margin(1e-5));
                }
            }
        }
    }

    // Cleanup
    delete global_time_param;
}

TEMPLATE_TEST_CASE("AudioRecordRenderStage - Multiple Renders Per Frame with Start/Stop Recording", 
                   "[audio_record_render_stage][gl_test][template][multiple_renders]", 
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

    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();

    REQUIRE(custom_generator.initialize());
    REQUIRE(record_stage.initialize());

    REQUIRE(custom_generator.connect_render_stage(&record_stage));

    context.prepare_draw();
    
    REQUIRE(custom_generator.bind());
    REQUIRE(record_stage.bind());

    std::cout << "\n=== Multiple Renders Per Frame Test ===" << std::endl;
    std::cout << "Rendering same frame multiple times with recording start/stop changes in between..." << std::endl;

    // Track recording state
    bool is_recording = false;
    unsigned int record_start_position = 0;

    for (int frame = 0; frame < TOTAL_FRAMES; frame++) {
        global_time_param->set_value(frame);
        global_time_param->render();

        // Render pass 1: Normal render
        custom_generator.render(frame);
        record_stage.render(frame);
        
        // INTERMEDIATE STATE CHANGES: Start/stop recording mid-frame
        if (frame == 5 && !is_recording) {
            std::cout << "Frame " << frame << ": Starting recording between renders at position 0" << std::endl;
            record_stage.record(0);
            is_recording = true;
            record_start_position = 0;
        } else if (frame == 15 && is_recording) {
            std::cout << "Frame " << frame << ": Stopping recording between renders" << std::endl;
            record_stage.stop();
            is_recording = false;
        } else if (frame == 20 && !is_recording) {
            std::cout << "Frame " << frame << ": Starting recording again between renders at position 10" << std::endl;
            record_stage.record(10);
            is_recording = true;
            record_start_position = 10;
        } else if (frame == 30 && is_recording) {
            std::cout << "Frame " << frame << ": Stopping recording again between renders" << std::endl;
            record_stage.stop();
            is_recording = false;
        }
        
        // Render pass 2: Same frame index, potentially different recording state
        custom_generator.render(frame);
        record_stage.render(frame);
    }

    // Ensure recording is stopped at the end
    if (is_recording) {
        record_stage.stop();
    }

    SECTION("Recorded Data Verification") {
        // Verify recorded data
        AudioTape* tape = record_stage.get_tape_new();
        REQUIRE(tape != nullptr);
        
        // Verify that frames 5-14 were recorded at position 0-9 (first recording session)
        for (int frame = 5; frame < 15; ++frame) {
            float expected = float(frame / 10) * 0.1f;
            unsigned int tape_position = frame - 5; // Position in tape (0-9)
            unsigned int sample_offset = tape_position * BUFFER_SIZE;
            
            auto playback_data = tape->playback(BUFFER_SIZE, sample_offset, false);
            REQUIRE(playback_data.size() == BUFFER_SIZE * NUM_CHANNELS);

            for (int i = 0; i < BUFFER_SIZE; ++i) {
                for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                    unsigned int idx = ch * BUFFER_SIZE + i;
                    REQUIRE(playback_data[idx] == Catch::Approx(expected).margin(1e-5));
                }
            }
        }

        // Verify that frames 20-29 were recorded at position 10-19 (second recording session)
        for (int frame = 20; frame < 30; ++frame) {
            float expected = float(frame / 10) * 0.1f;
            unsigned int tape_position = 10 + (frame - 20); // Position in tape (10-19)
            unsigned int sample_offset = tape_position * BUFFER_SIZE;
            
            auto playback_data = tape->playback(BUFFER_SIZE, sample_offset, false);
            REQUIRE(playback_data.size() == BUFFER_SIZE * NUM_CHANNELS);

            for (int i = 0; i < BUFFER_SIZE; ++i) {
                for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                    unsigned int idx = ch * BUFFER_SIZE + i;
                    REQUIRE(playback_data[idx] == Catch::Approx(expected).margin(1e-5));
                }
            }
        }

        // Verify positions before first recording (0-4) are zero
        for (int frame = 0; frame < 5; ++frame) {
            unsigned int tape_position = frame;
            unsigned int sample_offset = tape_position * BUFFER_SIZE;
            
            auto playback_data = tape->playback(BUFFER_SIZE, sample_offset, false);
            REQUIRE(playback_data.size() == BUFFER_SIZE * NUM_CHANNELS);

            for (int i = 0; i < BUFFER_SIZE; ++i) {
                for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                    unsigned int idx = ch * BUFFER_SIZE + i;
                    REQUIRE(playback_data[idx] == Catch::Approx(0.0f).margin(1e-5));
                }
            }
        }
    }

    // Cleanup
    delete global_time_param;
}
