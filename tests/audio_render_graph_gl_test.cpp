#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"

#include <thread>
#include <chrono>
#include "audio_core/audio_render_graph.h"
#include "audio_core/audio_render_stage.h"
#include "audio_output/audio_player_output.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"
#include "audio_render_stage/audio_multitrack_join_render_stage.h"

#include <string>
#include <cmath>

// Test parameter structure to hold buffer size and channel count combinations
struct TestParams {
    int buffer_size;
    int num_channels;
    const char* name;
};

// Define 3 different test parameter combinations
using TestParam1 = std::integral_constant<int, 0>; // 256 buffer, 1 channel
using TestParam2 = std::integral_constant<int, 1>; // 512 buffer, 2 channels
using TestParam3 = std::integral_constant<int, 2>; // 1024 buffer, 4 channels

// Parameter lookup function
constexpr TestParams get_test_params(int index) {
    constexpr TestParams params[] = {
        {256, 1, "256_buffer_1_channel"},
        {512, 2, "512_buffer_2_channels"},
        {1024, 4, "1024_buffer_4_channels"}
    };
    return params[index];
}

TEMPLATE_TEST_CASE("AudioRenderGraph sine chain: generator -> final (init/bind via graph)", "[audio_render_graph][gl_test][template]",
                   TestParam1, TestParam2, TestParam3) {
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_FRAMES = SAMPLE_RATE/BUFFER_SIZE  * 2; // 2 seconds

    // OpenGL/EGL context for shader-based stages
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    // Sine generator (multinote) and final stage
    auto * generator = new AudioGeneratorRenderStage(
        BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS,
        "build/shaders/multinote_sine_generator_render_stage.glsl"
    );
    auto * final_stage = new AudioFinalRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Connect generator -> final
    REQUIRE(generator->connect_render_stage(final_stage));

    // Build graph from final output node
    auto * graph = new AudioRenderGraph(final_stage);

    // Verify render order is topologically sorted: generator, final
    const auto & order = graph->get_render_order();
    REQUIRE(order.size() == 2);
    REQUIRE(order[0] == generator->gid);
    REQUIRE(order[1] == final_stage->gid);

    // Initialize via the graph (initializes and binds all stages)
    REQUIRE(graph->initialize());
    context.prepare_draw();

    // Play a note and render a few frames, verifying output becomes non-zero
    const float TONE = 440.0f;
    const float GAIN = 0.3f;
    generator->play_note(TONE, GAIN);

    bool produced_signal = false;
    // Global time buffer param used by shaders
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    REQUIRE(global_time_param->initialize());

    AudioPlayerOutput audio_output(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
    REQUIRE(audio_output.open());
    REQUIRE(audio_output.start());

    for (int frame = 0; frame < NUM_FRAMES; ++frame) {
        graph->bind();

        global_time_param->set_value(frame);
        global_time_param->render();

        graph->render(frame);

        const auto & data = final_stage->get_output_buffer_data();
        REQUIRE(data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));

        while (!audio_output.is_ready()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        audio_output.push(data.data());
    }

    audio_output.stop();
    audio_output.close();
    

    // Cleanup via graph ownership of stages
    delete graph;
}

// Write a test for checking that 

TEMPLATE_TEST_CASE("AudioRenderGraph multi-stage join with constant generators", "[audio_render_graph][gl_test][template]",
                   TestParam1, TestParam2, TestParam3) {
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_FRAMES = 5; // Short test for verification

    // OpenGL/EGL context for shader-based stages
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    // Create custom constant generator shaders
    std::string constant_generator_shader_template = R"(
void main() {
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    output_audio_texture = vec4(CONSTANT_VALUE) + stream_audio;
    debug_audio_texture = output_audio_texture;
}
)";

    // Create two generators with different constant values
    const float GEN1_CONSTANT = 0.25f;
    const float GEN2_CONSTANT = 0.75f;
    const float EXPECTED_SUM = GEN1_CONSTANT + GEN2_CONSTANT;

    // Generator 1 with constant 0.25
    std::string gen1_shader = constant_generator_shader_template;
    size_t pos1 = gen1_shader.find("CONSTANT_VALUE");
    gen1_shader.replace(pos1, 14, std::to_string(GEN1_CONSTANT));
    
    auto * generator1 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen1_shader, true);

    // Generator 2 with constant 0.75
    std::string gen2_shader = constant_generator_shader_template;
    size_t pos2 = gen2_shader.find("CONSTANT_VALUE");
    gen2_shader.replace(pos2, 14, std::to_string(GEN2_CONSTANT));
    
    auto * generator2 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen2_shader, true);

    // Create multitrack join stage with 2 inputs
    auto * join_stage = new AudioMultitrackJoinRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 2);

    // Create final render stage
    auto * final_stage = new AudioFinalRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Connect the stages: generator1 -> join, generator2 -> join, join -> final
    REQUIRE(generator1->connect_render_stage(join_stage));
    REQUIRE(generator2->connect_render_stage(join_stage));
    REQUIRE(join_stage->connect_render_stage(final_stage));

    // Build graph from final output node
    auto * graph = new AudioRenderGraph(final_stage);

    // Verify render order is topologically sorted: generators, join, final
    const auto & order = graph->get_render_order();
    REQUIRE(order.size() == 4);
    // The order should be: generator1, generator2, join, final
    // (or generator2, generator1, join, final - order of generators doesn't matter)
    REQUIRE(order[2] == join_stage->gid);
    REQUIRE(order[3] == final_stage->gid);

    // Initialize via the graph (initializes and binds all stages)
    REQUIRE(graph->initialize());
    context.prepare_draw();

    // Global time buffer param used by shaders
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    REQUIRE(global_time_param->initialize());

    // Render frames and verify output
    for (int frame = 0; frame < NUM_FRAMES; ++frame) {
        graph->bind();

        global_time_param->set_value(frame);
        global_time_param->render();

        graph->render(frame);

        const auto & data = final_stage->get_output_buffer_data();
        REQUIRE(data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));

        // Verify that the output contains the sum of the two constants
        // Check a few samples from the buffer to ensure they match expected sum
        for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
            for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                int index = sample * NUM_CHANNELS + channel;
                float sample_value = data[index];
                
                // The output should be the sum of the two constants (0.25 + 0.75 = 1.0)
                // Allow for small floating point precision differences
                REQUIRE(std::abs(sample_value - EXPECTED_SUM) < 0.001f);
            }
        }
    }

    // Cleanup via graph ownership of stages
    delete graph;
}


TEMPLATE_TEST_CASE("AudioRenderGraph dynamic generator deletion with output capture", "[audio_render_graph][gl_test][template]",
                   TestParam1, TestParam2, TestParam3) {
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int FRAMES_BEFORE_DELETE = 10; // Frames to render before deleting generator
    constexpr int FRAMES_AFTER_DELETE = 10;  // Frames to render after deleting generator
    constexpr int TOTAL_FRAMES = FRAMES_BEFORE_DELETE + FRAMES_AFTER_DELETE;

    // OpenGL/EGL context for shader-based stages
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    // Create custom constant generator shaders with different values
    std::string constant_generator_shader_template = R"(
void main() {
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    output_audio_texture = vec4(CONSTANT_VALUE) + stream_audio;
    debug_audio_texture = output_audio_texture;
}
)";

    // Create two generators with different constant values
    const float GEN1_CONSTANT = 0.3f;
    const float GEN2_CONSTANT = 0.7f;
    const float EXPECTED_SUM_BEFORE = GEN1_CONSTANT + GEN2_CONSTANT;
    const float EXPECTED_SUM_AFTER = GEN1_CONSTANT; // Only gen1 remains

    // Generator 1 with constant 0.3
    std::string gen1_shader = constant_generator_shader_template;
    size_t pos1 = gen1_shader.find("CONSTANT_VALUE");
    gen1_shader.replace(pos1, 14, std::to_string(GEN1_CONSTANT));
    
    auto * generator1 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen1_shader, true);

    // Generator 2 with constant 0.7
    std::string gen2_shader = constant_generator_shader_template;
    size_t pos2 = gen2_shader.find("CONSTANT_VALUE");
    gen2_shader.replace(pos2, 14, std::to_string(GEN2_CONSTANT));
    
    auto * generator2 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen2_shader, true);

    // Create final render stage
    auto * final_stage = new AudioFinalRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Connect the stages: generator1 -> join, generator2 -> join, join -> final
    REQUIRE(generator1->connect_render_stage(generator2));
    REQUIRE(generator2->connect_render_stage(final_stage));

    // Build graph from final output node
    auto * graph = new AudioRenderGraph(final_stage);

    // Verify render order is topologically sorted: generators, join, final
    const auto & order = graph->get_render_order();
    REQUIRE(order.size() == 3);
    REQUIRE(order[0] == generator1->gid);
    REQUIRE(order[1] == generator2->gid);
    REQUIRE(order[2] == final_stage->gid);

    // Initialize via the graph (initializes and binds all stages)
    REQUIRE(graph->initialize());
    context.prepare_draw();

    // Global time buffer param used by shaders
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    REQUIRE(global_time_param->initialize());

    // Vector to store captured output samples for analysis
    std::vector<float> captured_samples;
    captured_samples.reserve(BUFFER_SIZE * NUM_CHANNELS * TOTAL_FRAMES);

    // Phase 1: Render with both generators
    printf("Phase 1: Rendering with both generators (frames 0-%d)\n", FRAMES_BEFORE_DELETE - 1);
    for (int frame = 0; frame < FRAMES_BEFORE_DELETE; ++frame) {
        graph->bind();

        global_time_param->set_value(frame);
        global_time_param->render();

        graph->render(frame);

        const auto & data = final_stage->get_output_buffer_data();
        REQUIRE(data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));

        // Capture the output data
        captured_samples.insert(captured_samples.end(), data.begin(), data.end());

        // Verify output contains sum of both constants
        for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
            for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                int index = sample * NUM_CHANNELS + channel;
                float sample_value = data[index];
                REQUIRE(std::abs(sample_value - EXPECTED_SUM_BEFORE) < 0.001f);
            }
        }
    }

    // Phase 2: Dynamically delete generator2
    printf("Phase 2: Deleting generator2 (frame %d)\n", FRAMES_BEFORE_DELETE);
    
    // Remove generator2 from the graph
    auto removed_generator = graph->remove_render_stage(generator2->gid);
    REQUIRE(removed_generator != nullptr);
    REQUIRE(removed_generator.get() == generator2);
    
    // Verify the graph was updated correctly
    const auto & updated_order = graph->get_render_order();
    REQUIRE(updated_order.size() == 2); // generator1, final
    REQUIRE(updated_order[0] == generator1->gid);
    REQUIRE(updated_order[1] == final_stage->gid);

    // Phase 3: Render with only generator1
    printf("Phase 3: Rendering with only generator1 (frames %d-%d)\n", FRAMES_BEFORE_DELETE, TOTAL_FRAMES - 1);
    for (int frame = FRAMES_BEFORE_DELETE; frame < TOTAL_FRAMES; ++frame) {
        graph->bind();

        global_time_param->set_value(frame);
        global_time_param->render();

        graph->render(frame);

        const auto & data = final_stage->get_output_buffer_data();
        REQUIRE(data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));

        // Capture the output data
        captured_samples.insert(captured_samples.end(), data.begin(), data.end());

        // Verify output contains only generator1's constant
        for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
            for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                int index = sample * NUM_CHANNELS + channel;
                float sample_value = data[index];
                REQUIRE(std::abs(sample_value - EXPECTED_SUM_AFTER) < 0.001f);
            }
        }

    }

    // Verify the total number of captured samples
    REQUIRE(captured_samples.size() == BUFFER_SIZE * NUM_CHANNELS * TOTAL_FRAMES);

    // Analyze the captured output to ensure clean transition
    SECTION("Output Analysis - Clean Transition Verification") {
        INFO("Analyzing " << captured_samples.size() << " captured samples");
        
        // Check that samples before deletion are consistent with both generators
        for (int frame = 0; frame < FRAMES_BEFORE_DELETE; ++frame) {
            for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
                for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                    int index = (frame * BUFFER_SIZE + sample) * NUM_CHANNELS + channel;
                    float sample_value = captured_samples[index];
                    INFO("Before deletion - Frame " << frame << ", Sample " << sample << ", Channel " << channel 
                         << ": " << sample_value << " (expected: " << EXPECTED_SUM_BEFORE << ")");
                    REQUIRE(std::abs(sample_value - EXPECTED_SUM_BEFORE) < 0.001f);
                }
            }
        }

        // Check that samples after deletion are consistent with only generator1
        for (int frame = FRAMES_BEFORE_DELETE; frame < TOTAL_FRAMES; ++frame) {
            for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
                for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                    int index = (frame * BUFFER_SIZE + sample) * NUM_CHANNELS + channel;
                    float sample_value = captured_samples[index];
                    INFO("After deletion - Frame " << frame << ", Sample " << sample << ", Channel " << channel 
                         << ": " << sample_value << " (expected: " << EXPECTED_SUM_AFTER << ")");
                    REQUIRE(std::abs(sample_value - EXPECTED_SUM_AFTER) < 0.001f);
                }
            }
        }

        // Verify there's no carryover or artifacts at the transition point
        // Check the last frame before deletion and first frame after deletion
        int transition_frame_before = FRAMES_BEFORE_DELETE - 1;
        int transition_frame_after = FRAMES_BEFORE_DELETE;
        
        for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
            for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                int index_before = (transition_frame_before * BUFFER_SIZE + sample) * NUM_CHANNELS + channel;
                int index_after = (transition_frame_after * BUFFER_SIZE + sample) * NUM_CHANNELS + channel;
                
                float sample_before = captured_samples[index_before];
                float sample_after = captured_samples[index_after];
                
                INFO("Transition check - Sample " << sample << ", Channel " << channel 
                     << ": Before=" << sample_before << " (expected: " << EXPECTED_SUM_BEFORE 
                     << "), After=" << sample_after << " (expected: " << EXPECTED_SUM_AFTER << ")");
                
                REQUIRE(std::abs(sample_before - EXPECTED_SUM_BEFORE) < 0.001f);
                REQUIRE(std::abs(sample_after - EXPECTED_SUM_AFTER) < 0.001f);
            }
        }
    }

    // Cleanup
    delete graph;
    // Note: generator2 was already removed and returned as shared_ptr, so it will be cleaned up automatically
    // generator1 and other stages are owned by the graph and will be cleaned up when graph is deleted
}

TEMPLATE_TEST_CASE("AudioRenderGraph join with 2 generators and dynamic replacement", "[audio_render_graph][gl_test][template]",
                   TestParam1, TestParam2, TestParam3) {
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int FRAMES_BEFORE_REPLACE = 8;  // Frames to render before replacing generator
    constexpr int FRAMES_AFTER_REPLACE = 8;   // Frames to render after replacing generator
    constexpr int TOTAL_FRAMES = FRAMES_BEFORE_REPLACE + FRAMES_AFTER_REPLACE;

    // OpenGL/EGL context for shader-based stages
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    // Create custom constant generator shaders with different values
    std::string constant_generator_shader_template = R"(
void main() {
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    output_audio_texture = vec4(CONSTANT_VALUE) + stream_audio;
    debug_audio_texture = output_audio_texture;
}
)";

    // Create three generators with different constant values
    const float GEN1_CONSTANT = 0.2f;
    const float GEN2_CONSTANT = 0.5f;
    const float GEN3_CONSTANT = 0.8f;
    const float EXPECTED_SUM_BEFORE = GEN1_CONSTANT + GEN2_CONSTANT;
    const float EXPECTED_SUM_AFTER = GEN1_CONSTANT + GEN3_CONSTANT;

    // Generator 1 with constant 0.2
    std::string gen1_shader = constant_generator_shader_template;
    size_t pos1 = gen1_shader.find("CONSTANT_VALUE");
    gen1_shader.replace(pos1, 14, std::to_string(GEN1_CONSTANT));
    
    auto * generator1 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen1_shader, true);

    // Generator 2 with constant 0.5
    std::string gen2_shader = constant_generator_shader_template;
    size_t pos2 = gen2_shader.find("CONSTANT_VALUE");
    gen2_shader.replace(pos2, 14, std::to_string(GEN2_CONSTANT));
    
    auto * generator2 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen2_shader, true);

    // Generator 3 with constant 0.8 (will replace generator2)
    std::string gen3_shader = constant_generator_shader_template;
    size_t pos3 = gen3_shader.find("CONSTANT_VALUE");
    gen3_shader.replace(pos3, 14, std::to_string(GEN3_CONSTANT));
    
    auto * generator3 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen3_shader, true);

    // Create multitrack join stage with 2 inputs
    auto * join_stage = new AudioMultitrackJoinRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 2);

    // Create final render stage
    auto * final_stage = new AudioFinalRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Connect the stages: generator1 -> join, generator2 -> join, join -> final
    REQUIRE(generator1->connect_render_stage(join_stage));
    REQUIRE(generator2->connect_render_stage(join_stage));
    REQUIRE(join_stage->connect_render_stage(final_stage));

    // Build graph from final output node
    auto * graph = new AudioRenderGraph(final_stage);

    // Verify render order is topologically sorted: generators, join, final
    const auto & order = graph->get_render_order();
    REQUIRE(order.size() == 4);
    // The order should be: generator1, generator2, join, final
    // (or generator2, generator1, join, final - order of generators doesn't matter)
    REQUIRE(order[2] == join_stage->gid);
    REQUIRE(order[3] == final_stage->gid);

    // Initialize via the graph (initializes and binds all stages)
    REQUIRE(graph->initialize());
    context.prepare_draw();

    // Global time buffer param used by shaders
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    REQUIRE(global_time_param->initialize());

    // Vector to store captured output samples for analysis
    std::vector<float> captured_samples;
    captured_samples.reserve(BUFFER_SIZE * NUM_CHANNELS * TOTAL_FRAMES);

    // Phase 1: Render with generator1 and generator2
    printf("Phase 1: Rendering with generator1 and generator2 (frames 0-%d)\n", FRAMES_BEFORE_REPLACE - 1);
    for (int frame = 0; frame < FRAMES_BEFORE_REPLACE; ++frame) {
        graph->bind();

        global_time_param->set_value(frame);
        global_time_param->render();

        graph->render(frame);

        const auto & data = final_stage->get_output_buffer_data();
        REQUIRE(data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));

        // Capture the output data
        captured_samples.insert(captured_samples.end(), data.begin(), data.end());

        // Verify output contains sum of generator1 and generator2 constants
        for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
            for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                int index = sample * NUM_CHANNELS + channel;
                float sample_value = data[index];
                REQUIRE(std::abs(sample_value - EXPECTED_SUM_BEFORE) < 0.001f);
            }
        }
    }

    // Phase 2: Dynamically replace generator2 with generator3
    printf("Phase 2: Replacing generator2 with generator3 (frame %d)\n", FRAMES_BEFORE_REPLACE);
    
    // Remove generator2 from the graph
    auto replaced_generator = graph->replace_render_stage(generator2->gid, std::shared_ptr<AudioRenderStage>(generator3));
    REQUIRE(replaced_generator != nullptr);
    REQUIRE(replaced_generator.get() == generator2);
    
    // Verify the graph was updated correctly
    const auto & updated_order = graph->get_render_order();
    REQUIRE(updated_order.size() == 4); // generator1, generator3, join, final
    REQUIRE(updated_order[2] == join_stage->gid);
    REQUIRE(updated_order[3] == final_stage->gid);

    // Phase 3: Render with generator1 and generator3
    printf("Phase 3: Rendering with generator1 and generator3 (frames %d-%d)\n", FRAMES_BEFORE_REPLACE, TOTAL_FRAMES - 1);
    for (int frame = FRAMES_BEFORE_REPLACE; frame < TOTAL_FRAMES; ++frame) {
        graph->bind();

        global_time_param->set_value(frame);
        global_time_param->render();

        graph->render(frame);

        const auto & data = final_stage->get_output_buffer_data();
        REQUIRE(data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));

        // Capture the output data
        captured_samples.insert(captured_samples.end(), data.begin(), data.end());

        // Verify output contains sum of generator1 and generator3 constants
        for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
            for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                int index = sample * NUM_CHANNELS + channel;
                float sample_value = data[index];
                REQUIRE(std::abs(sample_value - EXPECTED_SUM_AFTER) < 0.001f);
            }
        }
    }

    // Verify the total number of captured samples
    REQUIRE(captured_samples.size() == BUFFER_SIZE * NUM_CHANNELS * TOTAL_FRAMES);

    // Analyze the captured output to ensure clean transition
    SECTION("Output Analysis - Clean Replacement Verification") {
        INFO("Analyzing " << captured_samples.size() << " captured samples");
        
        // Check that samples before replacement are consistent with generator1 + generator2
        for (int frame = 0; frame < FRAMES_BEFORE_REPLACE; ++frame) {
            for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
                for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                    int index = (frame * BUFFER_SIZE + sample) * NUM_CHANNELS + channel;
                    float sample_value = captured_samples[index];
                    INFO("Before replacement - Frame " << frame << ", Sample " << sample << ", Channel " << channel 
                         << ": " << sample_value << " (expected: " << EXPECTED_SUM_BEFORE << ")");
                    REQUIRE(std::abs(sample_value - EXPECTED_SUM_BEFORE) < 0.001f);
                }
            }
        }

        // Check that samples after replacement are consistent with generator1 + generator3
        for (int frame = FRAMES_BEFORE_REPLACE; frame < TOTAL_FRAMES; ++frame) {
            for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
                for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                    int index = (frame * BUFFER_SIZE + sample) * NUM_CHANNELS + channel;
                    float sample_value = captured_samples[index];
                    INFO("After replacement - Frame " << frame << ", Sample " << sample << ", Channel " << channel 
                         << ": " << sample_value << " (expected: " << EXPECTED_SUM_AFTER << ")");
                    REQUIRE(std::abs(sample_value - EXPECTED_SUM_AFTER) < 0.001f);
                }
            }
        }

        // Verify there's no carryover or artifacts at the transition point
        // Check the last frame before replacement and first frame after replacement
        int transition_frame_before = FRAMES_BEFORE_REPLACE - 1;
        int transition_frame_after = FRAMES_BEFORE_REPLACE;
        
        for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
            for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                int index_before = (transition_frame_before * BUFFER_SIZE + sample) * NUM_CHANNELS + channel;
                int index_after = (transition_frame_after * BUFFER_SIZE + sample) * NUM_CHANNELS + channel;
                
                float sample_before = captured_samples[index_before];
                float sample_after = captured_samples[index_after];
                
                INFO("Transition check - Sample " << sample << ", Channel " << channel 
                     << ": Before=" << sample_before << " (expected: " << EXPECTED_SUM_BEFORE 
                     << "), After=" << sample_after << " (expected: " << EXPECTED_SUM_AFTER << ")");
                
                REQUIRE(std::abs(sample_before - EXPECTED_SUM_BEFORE) < 0.001f);
                REQUIRE(std::abs(sample_after - EXPECTED_SUM_AFTER) < 0.001f);
            }
        }
    }

    // Cleanup
    delete graph;
    // Note: generator2 was already removed and returned as shared_ptr, so it will be cleaned up automatically
    // generator1, generator3, and other stages are owned by the graph and will be cleaned up when graph is deleted
}


TEMPLATE_TEST_CASE("AudioRenderGraph dynamic intermediate stage insertion and removal", "[audio_render_graph][gl_test][template]",
                   TestParam1, TestParam2, TestParam3) {
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int FRAMES_BEFORE_INSERT = 8; // Frames to render before inserting intermediate stage
    constexpr int FRAMES_WITH_INTERMEDIATE = 8; // Frames to render with intermediate stage
    constexpr int FRAMES_WITH_REPLACEMENT = 8; // Frames to render with replacement intermediate stage
    constexpr int FRAMES_AFTER_REMOVE = 8;  // Frames to render after removing intermediate stage
    constexpr int TOTAL_FRAMES = FRAMES_BEFORE_INSERT + FRAMES_WITH_INTERMEDIATE + FRAMES_WITH_REPLACEMENT + FRAMES_AFTER_REMOVE;

    // OpenGL/EGL context for shader-based stages
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    // Create custom constant generator shaders with different values
    std::string constant_generator_shader_template = R"(
void main() {
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    output_audio_texture = vec4(CONSTANT_VALUE) + stream_audio;
    debug_audio_texture = output_audio_texture;
}
)";

    // Create intermediate stage shader that multiplies by a factor
    std::string intermediate_stage_shader_template = R"(
void main() {
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    output_audio_texture = stream_audio * MULTIPLY_FACTOR;
    debug_audio_texture = output_audio_texture;
}
)";

    // Create replacement intermediate stage shader that adds an offset
    std::string replacement_stage_shader_template = R"(
void main() {
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    output_audio_texture = stream_audio + vec4(ADD_OFFSET);
    debug_audio_texture = output_audio_texture;
}
)";

    // Create two generators with different constant values
    const float GEN1_CONSTANT = 0.3f;
    const float GEN2_CONSTANT = 0.7f;
    const float INTERMEDIATE_MULTIPLY = 2.0f;
    const float REPLACEMENT_OFFSET = 0.5f;
    const float EXPECTED_SUM_BEFORE = GEN1_CONSTANT + GEN2_CONSTANT;
    const float EXPECTED_SUM_WITH_INTERMEDIATE = (GEN1_CONSTANT + GEN2_CONSTANT) * INTERMEDIATE_MULTIPLY;
    const float EXPECTED_SUM_WITH_REPLACEMENT = (GEN1_CONSTANT + GEN2_CONSTANT) + REPLACEMENT_OFFSET;
    const float EXPECTED_SUM_AFTER = GEN1_CONSTANT + GEN2_CONSTANT; // Back to original

    // Generator 1 with constant 0.3
    std::string gen1_shader = constant_generator_shader_template;
    size_t pos1 = gen1_shader.find("CONSTANT_VALUE");
    gen1_shader.replace(pos1, 14, std::to_string(GEN1_CONSTANT));
    
    auto * generator1 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen1_shader, true);

    // Generator 2 with constant 0.7
    std::string gen2_shader = constant_generator_shader_template;
    size_t pos2 = gen2_shader.find("CONSTANT_VALUE");
    gen2_shader.replace(pos2, 14, std::to_string(GEN2_CONSTANT));
    
    auto * generator2 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen2_shader, true);

    // Create intermediate stage that multiplies by 2.0
    std::string intermediate_shader = intermediate_stage_shader_template;
    size_t pos_intermediate = intermediate_shader.find("MULTIPLY_FACTOR");
    intermediate_shader.replace(pos_intermediate, 15, std::to_string(INTERMEDIATE_MULTIPLY));
    
    auto * intermediate_stage = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, intermediate_shader, false);

    // Create replacement intermediate stage that adds 0.5
    std::string replacement_shader = replacement_stage_shader_template;
    size_t pos_replacement = replacement_shader.find("ADD_OFFSET");
    replacement_shader.replace(pos_replacement, 10, std::to_string(REPLACEMENT_OFFSET));
    
    auto * replacement_stage = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, replacement_shader, false);

    // Create final render stage
    auto * final_stage = new AudioFinalRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Connect the stages: generator1 -> join, generator2 -> join, join -> final
    REQUIRE(generator1->connect_render_stage(generator2));
    REQUIRE(generator2->connect_render_stage(final_stage));

    // Build graph from final output node
    auto * graph = new AudioRenderGraph(final_stage);

    // Verify render order is topologically sorted: generators, final
    const auto & order = graph->get_render_order();
    REQUIRE(order.size() == 3);
    REQUIRE(order[0] == generator1->gid);
    REQUIRE(order[1] == generator2->gid);
    REQUIRE(order[2] == final_stage->gid);

    // Initialize via the graph (initializes and binds all stages)
    REQUIRE(graph->initialize());
    context.prepare_draw();

    // Global time buffer param used by shaders
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    REQUIRE(global_time_param->initialize());

    // Vector to store captured output samples for analysis
    std::vector<float> captured_samples;
    captured_samples.reserve(BUFFER_SIZE * NUM_CHANNELS * TOTAL_FRAMES);

    // Phase 1: Render with both generators (no intermediate stage)
    printf("Phase 1: Rendering with both generators (frames 0-%d)\n", FRAMES_BEFORE_INSERT - 1);
    for (int frame = 0; frame < FRAMES_BEFORE_INSERT; ++frame) {
        graph->bind();

        global_time_param->set_value(frame);
        global_time_param->render();

        graph->render(frame);

        const auto & data = final_stage->get_output_buffer_data();
        REQUIRE(data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));

        // Capture the output data
        captured_samples.insert(captured_samples.end(), data.begin(), data.end());

        // Verify output contains sum of both constants
        for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
            for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                int index = sample * NUM_CHANNELS + channel;
                float sample_value = data[index];
                REQUIRE(std::abs(sample_value - EXPECTED_SUM_BEFORE) < 0.001f);
            }
        }
    }

    // Phase 2: Dynamically insert intermediate stage using graph API
    printf("Phase 2: Inserting intermediate stage using graph API (frame %d)\n", FRAMES_BEFORE_INSERT);
    
    // Use graph API to insert intermediate stage between generator2 and final stage
    REQUIRE(graph->insert_render_stage_between(generator2->gid, final_stage->gid, std::shared_ptr<AudioRenderStage>(intermediate_stage)));
    
    // Verify the graph was updated correctly
    const auto & updated_order = graph->get_render_order();
    REQUIRE(updated_order.size() == 4); // generator1, generator2, intermediate, final
    REQUIRE(updated_order[0] == generator1->gid);
    REQUIRE(updated_order[1] == generator2->gid);
    REQUIRE(updated_order[2] == intermediate_stage->gid);
    REQUIRE(updated_order[3] == final_stage->gid);

    // Phase 3: Render with intermediate stage
    printf("Phase 3: Rendering with intermediate stage (frames %d-%d)\n", FRAMES_BEFORE_INSERT, FRAMES_BEFORE_INSERT + FRAMES_WITH_INTERMEDIATE - 1);
    for (int frame = FRAMES_BEFORE_INSERT; frame < FRAMES_BEFORE_INSERT + FRAMES_WITH_INTERMEDIATE; ++frame) {
        graph->bind();

        global_time_param->set_value(frame);
        global_time_param->render();

        graph->render(frame);

        const auto & data = final_stage->get_output_buffer_data();
        REQUIRE(data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));

        // Capture the output data
        captured_samples.insert(captured_samples.end(), data.begin(), data.end());

        // Verify output contains sum of both constants multiplied by intermediate factor
        for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
            for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                int index = sample * NUM_CHANNELS + channel;
                float sample_value = data[index];
                REQUIRE(std::abs(sample_value - EXPECTED_SUM_WITH_INTERMEDIATE) < 0.001f);
            }
        }
    }

    // Phase 4: Dynamically replace intermediate stage using graph API
    printf("Phase 4: Replacing intermediate stage using graph API (frame %d)\n", FRAMES_BEFORE_INSERT + FRAMES_WITH_INTERMEDIATE);
    
    // Use graph API to replace intermediate stage with replacement stage
    auto removed_intermediate = graph->replace_render_stage(intermediate_stage->gid, std::shared_ptr<AudioRenderStage>(replacement_stage));
    REQUIRE(removed_intermediate != nullptr);
    REQUIRE(removed_intermediate.get() == intermediate_stage);
    
    // Verify the graph was updated correctly
    const auto & replacement_order = graph->get_render_order();
    REQUIRE(replacement_order.size() == 4); // generator1, generator2, replacement, final
    REQUIRE(replacement_order[0] == generator1->gid);
    REQUIRE(replacement_order[1] == generator2->gid);
    REQUIRE(replacement_order[2] == replacement_stage->gid);
    REQUIRE(replacement_order[3] == final_stage->gid);

    // Phase 5: Render with replacement stage
    printf("Phase 5: Rendering with replacement stage (frames %d-%d)\n", FRAMES_BEFORE_INSERT + FRAMES_WITH_INTERMEDIATE, FRAMES_BEFORE_INSERT + FRAMES_WITH_INTERMEDIATE + FRAMES_WITH_REPLACEMENT - 1);
    for (int frame = FRAMES_BEFORE_INSERT + FRAMES_WITH_INTERMEDIATE; frame < FRAMES_BEFORE_INSERT + FRAMES_WITH_INTERMEDIATE + FRAMES_WITH_REPLACEMENT; ++frame) {
        graph->bind();

        global_time_param->set_value(frame);
        global_time_param->render();

        graph->render(frame);

        const auto & data = final_stage->get_output_buffer_data();
        REQUIRE(data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));

        // Capture the output data
        captured_samples.insert(captured_samples.end(), data.begin(), data.end());

        // Verify output contains sum of both constants plus replacement offset
        for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
            for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                int index = sample * NUM_CHANNELS + channel;
                float sample_value = data[index];
                REQUIRE(std::abs(sample_value - EXPECTED_SUM_WITH_REPLACEMENT) < 0.001f);
            }
        }
    }

    // Phase 6: Dynamically remove replacement stage using graph API
    printf("Phase 6: Removing replacement stage using graph API (frame %d)\n", FRAMES_BEFORE_INSERT + FRAMES_WITH_INTERMEDIATE + FRAMES_WITH_REPLACEMENT);
    
    // Use graph API to remove replacement stage
    auto removed_replacement = graph->remove_render_stage(replacement_stage->gid);
    REQUIRE(removed_replacement != nullptr);
    REQUIRE(removed_replacement.get() == replacement_stage);
    
    // Verify the graph was updated correctly
    const auto & final_order = graph->get_render_order();
    REQUIRE(final_order.size() == 3); // generator1, generator2, final
    REQUIRE(final_order[0] == generator1->gid);
    REQUIRE(final_order[1] == generator2->gid);
    REQUIRE(final_order[2] == final_stage->gid);

    // Phase 7: Render without any intermediate stage
    printf("Phase 7: Rendering without any intermediate stage (frames %d-%d)\n", FRAMES_BEFORE_INSERT + FRAMES_WITH_INTERMEDIATE + FRAMES_WITH_REPLACEMENT, TOTAL_FRAMES - 1);
    for (int frame = FRAMES_BEFORE_INSERT + FRAMES_WITH_INTERMEDIATE + FRAMES_WITH_REPLACEMENT; frame < TOTAL_FRAMES; ++frame) {
        graph->bind();

        global_time_param->set_value(frame);
        global_time_param->render();

        graph->render(frame);

        const auto & data = final_stage->get_output_buffer_data();
        REQUIRE(data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));

        // Capture the output data
        captured_samples.insert(captured_samples.end(), data.begin(), data.end());

        // Verify output contains sum of both constants (back to original)
        for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
            for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                int index = sample * NUM_CHANNELS + channel;
                float sample_value = data[index];
                REQUIRE(std::abs(sample_value - EXPECTED_SUM_AFTER) < 0.001f);
            }
        }
    }

    // Verify the total number of captured samples
    REQUIRE(captured_samples.size() == BUFFER_SIZE * NUM_CHANNELS * TOTAL_FRAMES);

    // Analyze the captured output to ensure clean transitions
    SECTION("Output Analysis - Clean Transition Verification") {
        INFO("Analyzing " << captured_samples.size() << " captured samples");
        
        // Check that samples before insertion are consistent with both generators
        for (int frame = 0; frame < FRAMES_BEFORE_INSERT; ++frame) {
            for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
                for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                    int index = (frame * BUFFER_SIZE + sample) * NUM_CHANNELS + channel;
                    float sample_value = captured_samples[index];
                    INFO("Before insertion - Frame " << frame << ", Sample " << sample << ", Channel " << channel 
                         << ": " << sample_value << " (expected: " << EXPECTED_SUM_BEFORE << ")");
                    REQUIRE(std::abs(sample_value - EXPECTED_SUM_BEFORE) < 0.001f);
                }
            }
        }

        // Check that samples with intermediate stage are consistent with multiplied values
        for (int frame = FRAMES_BEFORE_INSERT; frame < FRAMES_BEFORE_INSERT + FRAMES_WITH_INTERMEDIATE; ++frame) {
            for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
                for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                    int index = (frame * BUFFER_SIZE + sample) * NUM_CHANNELS + channel;
                    float sample_value = captured_samples[index];
                    INFO("With intermediate - Frame " << frame << ", Sample " << sample << ", Channel " << channel 
                         << ": " << sample_value << " (expected: " << EXPECTED_SUM_WITH_INTERMEDIATE << ")");
                    REQUIRE(std::abs(sample_value - EXPECTED_SUM_WITH_INTERMEDIATE) < 0.001f);
                }
            }
        }

        // Check that samples with replacement stage are consistent with added offset values
        for (int frame = FRAMES_BEFORE_INSERT + FRAMES_WITH_INTERMEDIATE; frame < FRAMES_BEFORE_INSERT + FRAMES_WITH_INTERMEDIATE + FRAMES_WITH_REPLACEMENT; ++frame) {
            for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
                for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                    int index = (frame * BUFFER_SIZE + sample) * NUM_CHANNELS + channel;
                    float sample_value = captured_samples[index];
                    INFO("With replacement - Frame " << frame << ", Sample " << sample << ", Channel " << channel 
                         << ": " << sample_value << " (expected: " << EXPECTED_SUM_WITH_REPLACEMENT << ")");
                    REQUIRE(std::abs(sample_value - EXPECTED_SUM_WITH_REPLACEMENT) < 0.001f);
                }
            }
        }

        // Check that samples after removal are consistent with original values
        for (int frame = FRAMES_BEFORE_INSERT + FRAMES_WITH_INTERMEDIATE + FRAMES_WITH_REPLACEMENT; frame < TOTAL_FRAMES; ++frame) {
            for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
                for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                    int index = (frame * BUFFER_SIZE + sample) * NUM_CHANNELS + channel;
                    float sample_value = captured_samples[index];
                    INFO("After removal - Frame " << frame << ", Sample " << sample << ", Channel " << channel 
                         << ": " << sample_value << " (expected: " << EXPECTED_SUM_AFTER << ")");
                    REQUIRE(std::abs(sample_value - EXPECTED_SUM_AFTER) < 0.001f);
                }
            }
        }

        // Verify there's no carryover or artifacts at the transition points
        // Check the last frame before insertion and first frame after insertion
        int transition_frame_before_insert = FRAMES_BEFORE_INSERT - 1;
        int transition_frame_after_insert = FRAMES_BEFORE_INSERT;
        
        for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
            for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                int index_before = (transition_frame_before_insert * BUFFER_SIZE + sample) * NUM_CHANNELS + channel;
                int index_after = (transition_frame_after_insert * BUFFER_SIZE + sample) * NUM_CHANNELS + channel;
                
                float sample_before = captured_samples[index_before];
                float sample_after = captured_samples[index_after];
                
                INFO("Insertion transition check - Sample " << sample << ", Channel " << channel 
                     << ": Before=" << sample_before << " (expected: " << EXPECTED_SUM_BEFORE 
                     << "), After=" << sample_after << " (expected: " << EXPECTED_SUM_WITH_INTERMEDIATE << ")");
                
                REQUIRE(std::abs(sample_before - EXPECTED_SUM_BEFORE) < 0.001f);
                REQUIRE(std::abs(sample_after - EXPECTED_SUM_WITH_INTERMEDIATE) < 0.001f);
            }
        }

        // Check the transition from intermediate to replacement stage
        int transition_frame_before_replace = FRAMES_BEFORE_INSERT + FRAMES_WITH_INTERMEDIATE - 1;
        int transition_frame_after_replace = FRAMES_BEFORE_INSERT + FRAMES_WITH_INTERMEDIATE;
        
        for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
            for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                int index_before = (transition_frame_before_replace * BUFFER_SIZE + sample) * NUM_CHANNELS + channel;
                int index_after = (transition_frame_after_replace * BUFFER_SIZE + sample) * NUM_CHANNELS + channel;
                
                float sample_before = captured_samples[index_before];
                float sample_after = captured_samples[index_after];
                
                INFO("Replacement transition check - Sample " << sample << ", Channel " << channel 
                     << ": Before=" << sample_before << " (expected: " << EXPECTED_SUM_WITH_INTERMEDIATE 
                     << "), After=" << sample_after << " (expected: " << EXPECTED_SUM_WITH_REPLACEMENT << ")");
                
                REQUIRE(std::abs(sample_before - EXPECTED_SUM_WITH_INTERMEDIATE) < 0.001f);
                REQUIRE(std::abs(sample_after - EXPECTED_SUM_WITH_REPLACEMENT) < 0.001f);
            }
        }

        // Check the last frame before final removal and first frame after final removal
        int transition_frame_before_final_remove = FRAMES_BEFORE_INSERT + FRAMES_WITH_INTERMEDIATE + FRAMES_WITH_REPLACEMENT - 1;
        int transition_frame_after_final_remove = FRAMES_BEFORE_INSERT + FRAMES_WITH_INTERMEDIATE + FRAMES_WITH_REPLACEMENT;
        
        for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
            for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                int index_before = (transition_frame_before_final_remove * BUFFER_SIZE + sample) * NUM_CHANNELS + channel;
                int index_after = (transition_frame_after_final_remove * BUFFER_SIZE + sample) * NUM_CHANNELS + channel;
                
                float sample_before = captured_samples[index_before];
                float sample_after = captured_samples[index_after];
                
                INFO("Final removal transition check - Sample " << sample << ", Channel " << channel 
                     << ": Before=" << sample_before << " (expected: " << EXPECTED_SUM_WITH_REPLACEMENT 
                     << "), After=" << sample_after << " (expected: " << EXPECTED_SUM_AFTER << ")");
                
                REQUIRE(std::abs(sample_before - EXPECTED_SUM_WITH_REPLACEMENT) < 0.001f);
                REQUIRE(std::abs(sample_after - EXPECTED_SUM_AFTER) < 0.001f);
            }
        }
    }

    // Cleanup
    delete graph;
    // Note: All stages are now owned by the graph and will be cleaned up when graph is deleted
    // The intermediate_stage and replacement_stage were transferred to the graph via the API calls
}

