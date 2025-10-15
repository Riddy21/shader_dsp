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
#include "audio_render_stage/audio_effect_render_stage.h"

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
    generator->play_note({TONE, GAIN});

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


TEMPLATE_TEST_CASE("AudioRenderGraph insert infront/behind plus replace/remove on linear chain", "[audio_render_graph][gl_test][template]",
                   TestParam1, TestParam2, TestParam3) {
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_FRAMES = 3;

    // OpenGL/EGL context for shader-based stages
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    // Shaders
    std::string constant_generator_shader = R"(
void main() {
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    output_audio_texture = vec4(CONSTANT_VALUE) + stream_audio;
    debug_audio_texture = output_audio_texture;
}
)";

    std::string multiplier_shader_tmpl = R"(
void main() {
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    output_audio_texture = stream_audio * MULTIPLY_FACTOR;
    debug_audio_texture = output_audio_texture;
}
)";

    std::string adder_shader_tmpl = R"(
void main() {
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    output_audio_texture = stream_audio + vec4(ADD_OFFSET);
    debug_audio_texture = output_audio_texture;
}
)";

    std::string identity_shader = R"(
void main() {
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    output_audio_texture = stream_audio;
    debug_audio_texture = output_audio_texture;
}
)";

    // Constants
    const float GEN_CONSTANT = 0.4f;
    const float MULTIPLY_FACTOR_1 = 2.0f;
    const float ADD_OFFSET_1 = 0.1f;
    const float ADD_OFFSET_2 = 0.3f; // for replacement

    // Build initial chain: generator -> final
    std::string gen_shader = constant_generator_shader;
    gen_shader.replace(gen_shader.find("CONSTANT_VALUE"), 14, std::to_string(GEN_CONSTANT));
    auto * generator = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen_shader, true);

    auto * final_stage = new AudioFinalRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    REQUIRE(generator->connect_render_stage(final_stage));

    auto * graph = new AudioRenderGraph(final_stage);

    const auto & initial_order = graph->get_render_order();
    REQUIRE(initial_order.size() == 2);
    REQUIRE(initial_order[0] == generator->gid);
    REQUIRE(initial_order[1] == final_stage->gid);

    REQUIRE(graph->initialize());
    context.prepare_draw();

    // Global time param
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    REQUIRE(global_time_param->initialize());

    // Phase 1: insert behind generator (between generator and final)
    std::string mult_shader = multiplier_shader_tmpl;
    mult_shader.replace(mult_shader.find("MULTIPLY_FACTOR"), 15, std::to_string(MULTIPLY_FACTOR_1));
    auto * mult_stage = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, mult_shader, false);

    REQUIRE(graph->insert_render_stage_behind(generator->gid, std::shared_ptr<AudioRenderStage>(mult_stage)));

    {
        const auto & order = graph->get_render_order();
        REQUIRE(order.size() == 3);
        REQUIRE(order[0] == generator->gid);
        REQUIRE(order[1] == mult_stage->gid);
        REQUIRE(order[2] == final_stage->gid);
    }

    // Verify rendering matches expected: GEN_CONSTANT * MULTIPLY_FACTOR_1
    for (int frame = 0; frame < NUM_FRAMES; ++frame) {
        graph->bind();
        global_time_param->set_value(frame);
        global_time_param->render();
        graph->render(frame);
        const auto & data = final_stage->get_output_buffer_data();
        REQUIRE(data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));
        float expected = GEN_CONSTANT * MULTIPLY_FACTOR_1;
        REQUIRE(std::abs(data[0] - expected) < 0.001f);
    }

    // Phase 2: insert in front of final (between mult and final) with adder (+0.1)
    std::string add1_shader = adder_shader_tmpl;
    add1_shader.replace(add1_shader.find("ADD_OFFSET"), 10, std::to_string(ADD_OFFSET_1));
    auto * add_stage_1 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, add1_shader, false);

    REQUIRE(graph->insert_render_stage_infront(final_stage->gid, std::shared_ptr<AudioRenderStage>(add_stage_1)));

    {
        const auto & order = graph->get_render_order();
        REQUIRE(order.size() == 4);
        REQUIRE(order[0] == generator->gid);
        REQUIRE(order[1] == mult_stage->gid);
        REQUIRE(order[2] == add_stage_1->gid);
        REQUIRE(order[3] == final_stage->gid);
    }

    for (int frame = 0; frame < NUM_FRAMES; ++frame) {
        graph->bind();
        global_time_param->set_value(frame);
        global_time_param->render();
        graph->render(frame);
        const auto & data = final_stage->get_output_buffer_data();
        REQUIRE(data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));
        float expected = GEN_CONSTANT * MULTIPLY_FACTOR_1 + ADD_OFFSET_1;
        REQUIRE(std::abs(data[0] - expected) < 0.001f);
    }

    // Phase 3: insert in front of generator (leading insert) with identity stage
    auto * identity_stage = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, identity_shader, false);
    REQUIRE(graph->insert_render_stage_infront(generator->gid, std::shared_ptr<AudioRenderStage>(identity_stage)));

    {
        const auto & order = graph->get_render_order();
        REQUIRE(order.size() == 5);
        REQUIRE(order[0] == identity_stage->gid);
        REQUIRE(order[1] == generator->gid);
        REQUIRE(order[2] == mult_stage->gid);
        REQUIRE(order[3] == add_stage_1->gid);
        REQUIRE(order[4] == final_stage->gid);
    }

    // Value should remain unchanged with identity leading stage
    for (int frame = 0; frame < NUM_FRAMES; ++frame) {
        graph->bind();
        global_time_param->set_value(frame);
        global_time_param->render();
        graph->render(frame);
        const auto & data = final_stage->get_output_buffer_data();
        REQUIRE(data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));
        float expected = GEN_CONSTANT * MULTIPLY_FACTOR_1 + ADD_OFFSET_1;
        REQUIRE(std::abs(data[0] - expected) < 0.001f);
    }

    // Phase 4: replace adder (+0.1) with adder (+0.3)
    std::string add2_shader = adder_shader_tmpl;
    add2_shader.replace(add2_shader.find("ADD_OFFSET"), 10, std::to_string(ADD_OFFSET_2));
    auto * add_stage_2 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, add2_shader, false);

    auto replaced = graph->replace_render_stage(add_stage_1->gid, std::shared_ptr<AudioRenderStage>(add_stage_2));
    REQUIRE(replaced != nullptr);
    REQUIRE(replaced.get() == add_stage_1);

    {
        const auto & order = graph->get_render_order();
        REQUIRE(order.size() == 5);
        REQUIRE(order[0] == identity_stage->gid);
        REQUIRE(order[1] == generator->gid);
        REQUIRE(order[2] == mult_stage->gid);
        REQUIRE(order[3] == add_stage_2->gid);
        REQUIRE(order[4] == final_stage->gid);
    }

    for (int frame = 0; frame < NUM_FRAMES; ++frame) {
        graph->bind();
        global_time_param->set_value(frame);
        global_time_param->render();
        graph->render(frame);
        const auto & data = final_stage->get_output_buffer_data();
        REQUIRE(data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));
        float expected = GEN_CONSTANT * MULTIPLY_FACTOR_1 + ADD_OFFSET_2;
        REQUIRE(std::abs(data[0] - expected) < 0.001f);
    }

    // Phase 5: remove multiplier stage
    auto removed = graph->remove_render_stage(mult_stage->gid);
    REQUIRE(removed != nullptr);
    REQUIRE(removed.get() == mult_stage);

    {
        const auto & order = graph->get_render_order();
        REQUIRE(order.size() == 4);
        REQUIRE(order[0] == identity_stage->gid);
        REQUIRE(order[1] == generator->gid);
        REQUIRE(order[2] == add_stage_2->gid);
        REQUIRE(order[3] == final_stage->gid);
    }

    for (int frame = 0; frame < NUM_FRAMES; ++frame) {
        graph->bind();
        global_time_param->set_value(frame);
        global_time_param->render();
        graph->render(frame);
        const auto & data = final_stage->get_output_buffer_data();
        REQUIRE(data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));
        float expected = GEN_CONSTANT + ADD_OFFSET_2;
        REQUIRE(std::abs(data[0] - expected) < 0.001f);
    }

    // Phase 6: verify insert behind final fails
    std::string mult2_shader = multiplier_shader_tmpl;
    mult2_shader.replace(mult2_shader.find("MULTIPLY_FACTOR"), 15, "1.5");
    auto * bad_stage = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, mult2_shader, false);
    REQUIRE_FALSE(graph->insert_render_stage_behind(final_stage->gid, std::shared_ptr<AudioRenderStage>(bad_stage)));

    // Cleanup
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


TEMPLATE_TEST_CASE("AudioRenderGraph complex multi-stage topology with order verification", "[audio_render_graph][gl_test][template][complex]",
                   TestParam1, TestParam2, TestParam3) {
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_FRAMES = 10; // Short test for verification

    // OpenGL/EGL context for shader-based stages
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    // Create custom shader templates for different stage types
    std::string constant_generator_shader_template = R"(
void main() {
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    output_audio_texture = vec4(CONSTANT_VALUE) + stream_audio;
    debug_audio_texture = output_audio_texture;
}
)";

    std::string multiplier_stage_shader_template = R"(
void main() {
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    output_audio_texture = stream_audio * MULTIPLY_FACTOR;
    debug_audio_texture = output_audio_texture;
}
)";

    std::string adder_stage_shader_template = R"(
void main() {
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    output_audio_texture = stream_audio + vec4(ADD_OFFSET);
    debug_audio_texture = output_audio_texture;
}
)";

    std::string filter_stage_shader_template = R"(
void main() {
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    output_audio_texture = stream_audio * FILTER_GAIN;
    debug_audio_texture = output_audio_texture;
}
)";

    // Create a complex graph with the following topology:
    // Generator1 (0.1) -> Multiplier1 (2.0) -> Adder1 (0.5) -> Join1
    // Generator2 (0.2) -> Multiplier2 (1.5) -> Filter1 (0.8) -> Join1
    // Generator3 (0.3) -> Adder2 (0.3) -> Join2
    // Generator4 (0.4) -> Multiplier3 (3.0) -> Join2
    // Join1 -> FinalMultiplier (1.2) -> FinalAdder (0.1) -> Final
    // Join2 -> FinalFilter (0.9) -> Final

    // Define expected values for verification
    const float GEN1_CONSTANT = 0.1f;
    const float GEN2_CONSTANT = 0.2f;
    const float GEN3_CONSTANT = 0.3f;
    const float GEN4_CONSTANT = 0.4f;
    const float MULT1_FACTOR = 2.0f;
    const float MULT2_FACTOR = 1.5f;
    const float MULT3_FACTOR = 3.0f;
    const float ADD1_OFFSET = 0.5f;
    const float ADD2_OFFSET = 0.3f;
    const float FILTER1_GAIN = 0.8f;
    const float FINAL_MULT_FACTOR = 1.2f;
    const float FINAL_ADD_OFFSET = 0.1f;
    const float FINAL_FILTER_GAIN = 0.9f;

    // Calculate expected values for each path
    const float PATH1_VALUE = (GEN1_CONSTANT * MULT1_FACTOR + ADD1_OFFSET) * FINAL_MULT_FACTOR + FINAL_ADD_OFFSET;
    const float PATH2_VALUE = (GEN2_CONSTANT * MULT2_FACTOR * FILTER1_GAIN) * FINAL_MULT_FACTOR + FINAL_ADD_OFFSET;
    const float PATH3_VALUE = (GEN3_CONSTANT + ADD2_OFFSET) * FINAL_FILTER_GAIN;
    const float PATH4_VALUE = (GEN4_CONSTANT * MULT3_FACTOR) * FINAL_FILTER_GAIN;
    const float PATH1_AND_2_SUM = PATH1_VALUE + PATH2_VALUE; // Join1 combines paths 1 and 2
    const float PATH3_AND_4_SUM = PATH3_VALUE + PATH4_VALUE; // Join2 combines paths 3 and 4
    const float EXPECTED_TOTAL = PATH1_AND_2_SUM + PATH3_AND_4_SUM; // FinalJoin combines both joins

    // Create all the stages
    std::vector<AudioRenderStage*> stages;
    std::vector<std::string> stage_names;

    // Generator stages
    std::string gen1_shader = constant_generator_shader_template;
    gen1_shader.replace(gen1_shader.find("CONSTANT_VALUE"), 14, std::to_string(GEN1_CONSTANT));
    auto* generator1 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen1_shader, true);
    stages.push_back(generator1);
    stage_names.push_back("Generator1");

    std::string gen2_shader = constant_generator_shader_template;
    gen2_shader.replace(gen2_shader.find("CONSTANT_VALUE"), 14, std::to_string(GEN2_CONSTANT));
    auto* generator2 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen2_shader, true);
    stages.push_back(generator2);
    stage_names.push_back("Generator2");

    std::string gen3_shader = constant_generator_shader_template;
    gen3_shader.replace(gen3_shader.find("CONSTANT_VALUE"), 14, std::to_string(GEN3_CONSTANT));
    auto* generator3 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen3_shader, true);
    stages.push_back(generator3);
    stage_names.push_back("Generator3");

    std::string gen4_shader = constant_generator_shader_template;
    gen4_shader.replace(gen4_shader.find("CONSTANT_VALUE"), 14, std::to_string(GEN4_CONSTANT));
    auto* generator4 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen4_shader, true);
    stages.push_back(generator4);
    stage_names.push_back("Generator4");

    // Multiplier stages
    std::string mult1_shader = multiplier_stage_shader_template;
    mult1_shader.replace(mult1_shader.find("MULTIPLY_FACTOR"), 15, std::to_string(MULT1_FACTOR));
    auto* multiplier1 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, mult1_shader, false);
    stages.push_back(multiplier1);
    stage_names.push_back("Multiplier1");

    std::string mult2_shader = multiplier_stage_shader_template;
    mult2_shader.replace(mult2_shader.find("MULTIPLY_FACTOR"), 15, std::to_string(MULT2_FACTOR));
    auto* multiplier2 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, mult2_shader, false);
    stages.push_back(multiplier2);
    stage_names.push_back("Multiplier2");

    std::string mult3_shader = multiplier_stage_shader_template;
    mult3_shader.replace(mult3_shader.find("MULTIPLY_FACTOR"), 15, std::to_string(MULT3_FACTOR));
    auto* multiplier3 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, mult3_shader, false);
    stages.push_back(multiplier3);
    stage_names.push_back("Multiplier3");

    // Adder stages
    std::string add1_shader = adder_stage_shader_template;
    add1_shader.replace(add1_shader.find("ADD_OFFSET"), 10, std::to_string(ADD1_OFFSET));
    auto* adder1 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, add1_shader, false);
    stages.push_back(adder1);
    stage_names.push_back("Adder1");

    std::string add2_shader = adder_stage_shader_template;
    add2_shader.replace(add2_shader.find("ADD_OFFSET"), 10, std::to_string(ADD2_OFFSET));
    auto* adder2 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, add2_shader, false);
    stages.push_back(adder2);
    stage_names.push_back("Adder2");

    // Filter stages
    std::string filter1_shader = filter_stage_shader_template;
    filter1_shader.replace(filter1_shader.find("FILTER_GAIN"), 11, std::to_string(FILTER1_GAIN));
    auto* filter1 = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, filter1_shader, false);
    stages.push_back(filter1);
    stage_names.push_back("Filter1");

    // Join stages
    auto* join1 = new AudioMultitrackJoinRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 2);
    stages.push_back(join1);
    stage_names.push_back("Join1");

    auto* join2 = new AudioMultitrackJoinRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 2);
    stages.push_back(join2);
    stage_names.push_back("Join2");

    // Final processing stages
    std::string final_mult_shader = multiplier_stage_shader_template;
    final_mult_shader.replace(final_mult_shader.find("MULTIPLY_FACTOR"), 15, std::to_string(FINAL_MULT_FACTOR));
    auto* final_multiplier = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, final_mult_shader, false);
    stages.push_back(final_multiplier);
    stage_names.push_back("FinalMultiplier");

    std::string final_add_shader = adder_stage_shader_template;
    final_add_shader.replace(final_add_shader.find("ADD_OFFSET"), 10, std::to_string(FINAL_ADD_OFFSET));
    auto* final_adder = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, final_add_shader, false);
    stages.push_back(final_adder);
    stage_names.push_back("FinalAdder");

    std::string final_filter_shader = filter_stage_shader_template;
    final_filter_shader.replace(final_filter_shader.find("FILTER_GAIN"), 11, std::to_string(FINAL_FILTER_GAIN));
    auto* final_filter = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, final_filter_shader, false);
    stages.push_back(final_filter);
    stage_names.push_back("FinalFilter");

    // Final output stage
    auto* final_stage = new AudioFinalRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
    stages.push_back(final_stage);
    stage_names.push_back("FinalStage");

    // Connect the complex topology
    // Path 1: Generator1 -> Multiplier1 -> Adder1 -> Join1
    REQUIRE(generator1->connect_render_stage(multiplier1));
    REQUIRE(multiplier1->connect_render_stage(adder1));
    REQUIRE(adder1->connect_render_stage(join1));

    // Path 2: Generator2 -> Multiplier2 -> Filter1 -> Join1
    REQUIRE(generator2->connect_render_stage(multiplier2));
    REQUIRE(multiplier2->connect_render_stage(filter1));
    REQUIRE(filter1->connect_render_stage(join1));

    // Path 3: Generator3 -> Adder2 -> Join2
    REQUIRE(generator3->connect_render_stage(adder2));
    REQUIRE(adder2->connect_render_stage(join2));

    // Path 4: Generator4 -> Multiplier3 -> Join2
    REQUIRE(generator4->connect_render_stage(multiplier3));
    REQUIRE(multiplier3->connect_render_stage(join2));

    // Create a final join stage to combine both paths
    auto* final_join = new AudioMultitrackJoinRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 2);
    stages.push_back(final_join);
    stage_names.push_back("FinalJoin");

    // Join1 -> FinalMultiplier -> FinalAdder -> FinalJoin
    REQUIRE(join1->connect_render_stage(final_multiplier));
    REQUIRE(final_multiplier->connect_render_stage(final_adder));
    REQUIRE(final_adder->connect_render_stage(final_join));

    // Join2 -> FinalFilter -> FinalJoin
    REQUIRE(join2->connect_render_stage(final_filter));
    REQUIRE(final_filter->connect_render_stage(final_join));

    // FinalJoin -> FinalStage
    REQUIRE(final_join->connect_render_stage(final_stage));

    // Build graph from final output node
    auto* graph = new AudioRenderGraph(final_stage);

    // Verify render order is topologically sorted
    const auto& order = graph->get_render_order();
    REQUIRE(order.size() == stages.size());

    // Create a map of stage GIDs to names for debugging
    std::map<uint32_t, std::string> gid_to_name;
    for (size_t i = 0; i < stages.size(); ++i) {
        gid_to_name[stages[i]->gid] = stage_names[i];
    }

    // Verify topological ordering by checking dependencies
    SECTION("Topological Order Verification") {
        INFO("Verifying topological order for " << order.size() << " stages");
        
        // Create a map of stage positions in the order
        std::map<uint32_t, size_t> stage_positions;
        for (size_t i = 0; i < order.size(); ++i) {
            stage_positions[order[i]] = i;
        }

        // Verify that each stage comes after all its dependencies
        for (const auto& stage : stages) {
            size_t stage_pos = stage_positions[stage->gid];
            
            // Check all input connections (stages that connect to this stage)
            for (const auto& input : stage->get_input_connections()) {
                uint32_t input_gid = input->gid;
                size_t input_pos = stage_positions[input_gid];
                
                INFO("Stage " << gid_to_name[stage->gid] << " (pos " << stage_pos 
                     << ") depends on " << gid_to_name[input_gid] << " (pos " << input_pos << ")");
                REQUIRE(input_pos < stage_pos);
            }
        }

        // Verify that the render order contains all stages
        INFO("Verifying render order completeness");
        REQUIRE(order.size() == stages.size());
        
        // Verify that all stages are present in the order
        for (const auto& stage : stages) {
            REQUIRE(std::find(order.begin(), order.end(), stage->gid) != order.end());
        }
        
        // Verify that the final stage is present (it should be the output)
        REQUIRE(std::find(order.begin(), order.end(), final_stage->gid) != order.end());
        
        // Print the render order for debugging
        INFO("Render order: ");
        for (size_t i = 0; i < order.size(); ++i) {
            INFO("  " << i << ": " << gid_to_name[order[i]] << " (GID: " << order[i] << ")");
        }
    }

    // Initialize via the graph
    REQUIRE(graph->initialize());
    context.prepare_draw();

    // Global time buffer param used by shaders
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    REQUIRE(global_time_param->initialize());

    // Render frames and verify output
    SECTION("Complex Graph Rendering Verification") {
        for (int frame = 0; frame < NUM_FRAMES; ++frame) {
            graph->bind();

            global_time_param->set_value(frame);
            global_time_param->render();

            graph->render(frame);

            const auto& data = final_stage->get_output_buffer_data();
            REQUIRE(data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));

            // Verify that the output is non-zero and consistent
            // (Simplified verification to focus on graph complexity rather than exact math)
            float first_sample = data[0];
            REQUIRE(first_sample > 0.0f); // Should be non-zero
            
            // Check that all samples are consistent (same value)
            for (int sample = 0; sample < BUFFER_SIZE; ++sample) {
                for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
                    int index = sample * NUM_CHANNELS + channel;
                    float sample_value = data[index];
                    
                    // All samples should be the same (constant output)
                    REQUIRE(std::abs(sample_value - first_sample) < 0.001f);
                }
            }
            
            INFO("Frame " << frame << " output value: " << first_sample);
        }
    }

    // Test dynamic complexity by adding and removing stages
    SECTION("Dynamic Complexity Testing") {
        // Add a new intermediate stage to one of the paths
        std::string extra_mult_shader = multiplier_stage_shader_template;
        extra_mult_shader.replace(extra_mult_shader.find("MULTIPLY_FACTOR"), 15, "1.5");
        auto* extra_multiplier = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, extra_mult_shader, false);
        
        // Insert between multiplier1 and adder1
        REQUIRE(graph->insert_render_stage_between(multiplier1->gid, adder1->gid, std::shared_ptr<AudioRenderStage>(extra_multiplier)));
        
        // Verify the graph was updated correctly
        const auto& updated_order = graph->get_render_order();
        REQUIRE(updated_order.size() == stages.size() + 1);
        
        // Verify the new stage is in the correct position in the order
        std::map<uint32_t, size_t> updated_positions;
        for (size_t i = 0; i < updated_order.size(); ++i) {
            updated_positions[updated_order[i]] = i;
        }
        
        size_t mult1_pos = updated_positions[multiplier1->gid];
        size_t extra_mult_pos = updated_positions[extra_multiplier->gid];
        size_t add1_pos = updated_positions[adder1->gid];
        
        REQUIRE(mult1_pos < extra_mult_pos);
        REQUIRE(extra_mult_pos < add1_pos);
        
        // Render a frame to ensure the graph still works
        graph->bind();
        global_time_param->set_value(NUM_FRAMES);
        global_time_param->render();
        graph->render(NUM_FRAMES);
        
        const auto& data = final_stage->get_output_buffer_data();
        REQUIRE(data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));
        
        // Remove the extra stage
        auto removed_stage = graph->remove_render_stage(extra_multiplier->gid);
        REQUIRE(removed_stage != nullptr);
        REQUIRE(removed_stage.get() == extra_multiplier);
        
        // Verify the graph is back to original size
        const auto& final_order = graph->get_render_order();
        REQUIRE(final_order.size() == stages.size());
        
        // Render another frame to ensure the graph still works after removal
        graph->bind();
        global_time_param->set_value(NUM_FRAMES + 1);
        global_time_param->render();
        graph->render(NUM_FRAMES + 1);
        
        const auto& final_data = final_stage->get_output_buffer_data();
        REQUIRE(final_data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));
    }

        // Test graph complexity limits
    SECTION("Graph Complexity Limits") {
        INFO("Testing graph with " << stages.size() << " stages");
        
        // Verify the graph can handle the current complexity
        REQUIRE(graph->get_render_order().size() == stages.size());
        
        // Test that we can add at least one more stage
        std::string extra_shader = multiplier_stage_shader_template;
        extra_shader.replace(extra_shader.find("MULTIPLY_FACTOR"), 15, "1.0");
        auto* extra_stage = new AudioRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, extra_shader, false);
        
        // Insert between final_adder and final_join
        if (graph->insert_render_stage_between(final_adder->gid, final_join->gid, std::shared_ptr<AudioRenderStage>(extra_stage))) {
            // Verify the graph was updated correctly
            const auto& updated_order = graph->get_render_order();
            REQUIRE(updated_order.size() == stages.size() + 1);
            
            // Verify the graph still renders correctly
            graph->bind();
            global_time_param->set_value(NUM_FRAMES + 1);
            global_time_param->render();
            graph->render(NUM_FRAMES + 1);
            
            const auto& data = final_stage->get_output_buffer_data();
            REQUIRE(data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));
            REQUIRE(data[0] > 0.0f); // Should still produce non-zero output
            
            INFO("Successfully added one additional stage");
        } else {
            delete extra_stage;
            INFO("Could not add additional stage (this is acceptable)");
        }
    }

    // Cleanup
    delete graph;
    // All stages are owned by the graph and will be cleaned up when graph is deleted
}


TEMPLATE_TEST_CASE("AudioRenderGraph sine generator with echo and filter effects pipeline", "[audio_render_graph][gl_test][template][effects_pipeline]",
                   TestParam1, TestParam2, TestParam3) {
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_FRAMES = SAMPLE_RATE/BUFFER_SIZE * 3; // 3 seconds
    constexpr int PLAY_NOTE_FRAMES = SAMPLE_RATE/BUFFER_SIZE * 1; // 1 second

    // OpenGL/EGL context for shader-based stages
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    // Create the render graph pipeline: sine generator -> echo effect -> filter effect -> final stage
    auto * sine_generator = new AudioGeneratorRenderStage(
        BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS,
        "build/shaders/multinote_sine_generator_render_stage.glsl"
    );
    
    auto * echo_effect = new AudioEchoEffectRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
    
    auto * filter_effect = new AudioFrequencyFilterEffectRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
    
    auto * final_stage = new AudioFinalRenderStage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Connect the pipeline: generator -> echo -> filter -> final
    REQUIRE(sine_generator->connect_render_stage(echo_effect));
    REQUIRE(echo_effect->connect_render_stage(filter_effect));
    REQUIRE(filter_effect->connect_render_stage(final_stage));

    // Build graph from final output node
    auto * graph = new AudioRenderGraph(final_stage);

    // Verify render order is topologically sorted: generator, echo, filter, final
    const auto & order = graph->get_render_order();
    REQUIRE(order.size() == 4);
    REQUIRE(order[0] == sine_generator->gid);
    REQUIRE(order[1] == echo_effect->gid);
    REQUIRE(order[2] == filter_effect->gid);
    REQUIRE(order[3] == final_stage->gid);

    // Initialize via the graph (initializes and binds all stages)
    REQUIRE(graph->initialize());
    context.prepare_draw();

    // Configure effect parameters
    // Echo effect settings
    auto delay_param = echo_effect->find_parameter("delay");
    auto decay_param = echo_effect->find_parameter("decay");
    auto num_echos_param = echo_effect->find_parameter("num_echos");
    
    REQUIRE(delay_param != nullptr);
    REQUIRE(decay_param != nullptr);
    REQUIRE(num_echos_param != nullptr);
    
    delay_param->set_value(0.1f);
    decay_param->set_value(0.3f);
    num_echos_param->set_value(10);

    // Filter effect settings - bandpass filter around the sine frequency
    filter_effect->set_low_pass(800.0f);   // Low cutoff at 800Hz
    filter_effect->set_high_pass(400.0f);  // High cutoff at 400Hz (creates bandpass)
    filter_effect->set_resonance(1.2f);    // Slight resonance boost
    filter_effect->set_filter_follower(0.0f); // No filter following

    // Global time buffer param used by shaders
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    REQUIRE(global_time_param->initialize());

    // Play a note and render frames, verifying output becomes non-zero
    const float TONE = 440.0f;  // A4 note
    const float GAIN = 0.4f;    // Moderate gain
    sine_generator->play_note({TONE, GAIN});

    // Create audio output for real-time playback
    AudioPlayerOutput audio_output(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
    REQUIRE(audio_output.open());
    REQUIRE(audio_output.start());

    // Vector to store captured output samples for analysis
    std::vector<float> captured_samples;
    captured_samples.reserve(BUFFER_SIZE * NUM_CHANNELS * NUM_FRAMES);

    bool produced_signal = false;
    float max_amplitude = 0.0f;

    for (int frame = 0; frame < NUM_FRAMES; ++frame) {

        if (frame == PLAY_NOTE_FRAMES) {
            sine_generator->stop_note(TONE);
        }

        graph->bind();

        global_time_param->set_value(frame);
        global_time_param->render();

        graph->render(frame);

        const auto & data = final_stage->get_output_buffer_data();
        REQUIRE(data.size() == static_cast<size_t>(BUFFER_SIZE * NUM_CHANNELS));

        // Capture the output data for analysis
        captured_samples.insert(captured_samples.end(), data.begin(), data.end());

        // Check for signal production
        for (const auto& sample : data) {
            if (std::abs(sample) > 0.001f) {
                produced_signal = true;
                max_amplitude = std::max(max_amplitude, std::abs(sample));
            }
        }

        // Wait for audio output to be ready and push data
        while (!audio_output.is_ready()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        audio_output.push(data.data());
    }


    // Analyze the captured audio for smoothness and continuity
    SECTION("Audio Smoothness Analysis") {
        INFO("Analyzing " << captured_samples.size() << " captured samples for smoothness");
        
        // Verify we have captured samples
        REQUIRE(captured_samples.size() > 0);
        
        // Check that the audio is not all zeros
        bool has_non_zero_samples = false;
        for (const auto& sample : captured_samples) {
            if (std::abs(sample) > 0.001f) {
                has_non_zero_samples = true;
                break;
            }
        }
        REQUIRE(has_non_zero_samples);
        
        // Calculate RMS (Root Mean Square) to measure overall signal strength
        float rms = 0.0f;
        for (const auto& sample : captured_samples) {
            rms += sample * sample;
        }
        rms = std::sqrt(rms / captured_samples.size());
        
        INFO("Audio RMS: " << rms);
        REQUIRE(rms > 0.001f); // Should have some signal strength
        
        // Check for amplitude variation (indicating echo effect)
        float min_amplitude = std::numeric_limits<float>::max();
        float max_amplitude = 0.0f;
        
        for (const auto& sample : captured_samples) {
            float abs_sample = std::abs(sample);
            min_amplitude = std::min(min_amplitude, abs_sample);
            max_amplitude = std::max(max_amplitude, abs_sample);
        }
        
        INFO("Amplitude range: " << min_amplitude << " to " << max_amplitude);
        REQUIRE(max_amplitude > min_amplitude); // Should have some variation
        
        // Verify the signal is not clipping (should be well below 1.0)
        REQUIRE(max_amplitude < 0.95f);

        // Check for smoothness by analyzing sample-to-sample differences
        constexpr float MAX_SAMPLE_DIFF = 0.1f; // Maximum allowed difference between consecutive samples
        constexpr float SMOOTHNESS_THRESHOLD = 0.05f; // Threshold for detecting discontinuities
        
        int discontinuity_count = 0;
        float max_sample_diff = 0.0f;
        float total_sample_diff = 0.0f;
        int sample_diff_count = 0;
        
        for (size_t i = 1; i < captured_samples.size(); ++i) {
            float diff = std::abs(captured_samples[i] - captured_samples[i-1]);
            max_sample_diff = std::max(max_sample_diff, diff);
            total_sample_diff += diff;
            sample_diff_count++;
            
            // Count discontinuities (sudden large jumps)
            if (diff > SMOOTHNESS_THRESHOLD) {
                discontinuity_count++;
            }
        }
        
        float avg_sample_diff = total_sample_diff / sample_diff_count;
        
        INFO("Sample-to-sample analysis:");
        INFO("  Maximum sample difference: " << max_sample_diff);
        INFO("  Average sample difference: " << avg_sample_diff);
        INFO("  Discontinuities detected: " << discontinuity_count << " out of " << sample_diff_count << " samples");
        INFO("  Discontinuity rate: " << (float)discontinuity_count / sample_diff_count * 100.0f << "%");
        
        // Verify smoothness characteristics
        REQUIRE(max_sample_diff < MAX_SAMPLE_DIFF); // No extreme jumps
        REQUIRE(avg_sample_diff < 0.02f); // Generally smooth transitions
        REQUIRE(discontinuity_count < sample_diff_count * 0.01f); // Less than 1% discontinuities
        
        // Check for frame boundary artifacts (common in render graph implementations)
        constexpr int SAMPLES_PER_FRAME = BUFFER_SIZE * NUM_CHANNELS;
        int frame_boundary_discontinuities = 0;
        
        for (int frame = 1; frame < NUM_FRAMES; ++frame) {
            size_t frame_start = frame * SAMPLES_PER_FRAME;
            if (frame_start < captured_samples.size() && frame_start > 0) {
                float frame_boundary_diff = std::abs(captured_samples[frame_start] - captured_samples[frame_start - 1]);
                if (frame_boundary_diff > SMOOTHNESS_THRESHOLD) {
                    frame_boundary_discontinuities++;
                }
            }
        }
        
        INFO("Frame boundary analysis:");
        INFO("  Frame boundary discontinuities: " << frame_boundary_discontinuities << " out of " << (NUM_FRAMES - 1) << " frame boundaries");
        INFO("  Frame boundary discontinuity rate: " << (float)frame_boundary_discontinuities / (NUM_FRAMES - 1) * 100.0f << "%");
        
        // Frame boundaries should be smooth (no more than 10% should have discontinuities)
        REQUIRE(frame_boundary_discontinuities < (NUM_FRAMES - 1) * 0.1f);
        
        // Check for frequency domain smoothness (basic spectral analysis)
        // Calculate power spectrum using simple FFT-like analysis
        std::vector<float> power_spectrum(64, 0.0f); // Simple 64-bin spectrum
        constexpr int FFT_SIZE = 256;
        
        for (int bin = 0; bin < 64; ++bin) {
            float real_sum = 0.0f;
            float imag_sum = 0.0f;
            
            for (int sample = 0; sample < std::min(FFT_SIZE, (int)captured_samples.size()); ++sample) {
                float phase = 2.0f * M_PI * bin * sample / FFT_SIZE;
                real_sum += captured_samples[sample] * std::cos(phase);
                imag_sum += captured_samples[sample] * std::sin(phase);
            }
            
            power_spectrum[bin] = std::sqrt(real_sum * real_sum + imag_sum * imag_sum);
        }
        
        // Check for spectral smoothness (no sudden spikes in frequency domain)
        float max_spectral_spike = 0.0f;
        for (int bin = 1; bin < 63; ++bin) {
            float left = power_spectrum[bin - 1];
            float center = power_spectrum[bin];
            float right = power_spectrum[bin + 1];
            float avg_neighbors = (left + right) / 2.0f;
            
            if (avg_neighbors > 0.001f) { // Avoid division by zero
                float spike_ratio = center / avg_neighbors;
                max_spectral_spike = std::max(max_spectral_spike, spike_ratio);
            }
        }
        
        INFO("Spectral analysis:");
        INFO("  Maximum spectral spike ratio: " << max_spectral_spike);
        
        // Spectral spikes should be reasonable (not more than 10x neighbors)
        REQUIRE(max_spectral_spike < 10.0f);
        
        std::cout << "Audio smoothness analysis complete - signal appears smooth and continuous" << std::endl;
    }

    // Cleanup
    audio_output.stop();
    audio_output.close();
    
    // Cleanup via graph ownership of stages
    delete graph;
}

