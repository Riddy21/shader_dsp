#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"

#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_render_stage/audio_multitrack_join_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <type_traits>

// Test parameter structure to hold buffer size and channel count combinations
struct TestParams {
    int buffer_size;
    int num_channels;
    const char* name;
};

// Update parameters to focus on single channel
using TestParam1 = std::integral_constant<int, 0>; // 256 buffer, 1 channel
using TestParam2 = std::integral_constant<int, 1>; // 512 buffer, 2 channels
using TestParam3 = std::integral_constant<int, 2>; // 1024 buffer, 4 channels

constexpr TestParams get_test_params(int index) {
    constexpr TestParams params[] = {
        {256, 1, "256_buffer_1_channel"},
        {512, 2, "512_buffer_2_channels"},
        {1024, 4, "1024_buffer_4_channels"}
    };
    return params[index];
}

// Test parameter structure for variable inputs test
struct VariableInputTestParams {
    int buffer_size;
    int num_channels;
    int num_inputs;
    const char* name;
};

// Parameters for variable inputs test
using VariableTestParam1 = std::integral_constant<int, 0>; // 256 buffer, 1 channel, 1 input
using VariableTestParam2 = std::integral_constant<int, 1>; // 512 buffer, 2 channels, 3 inputs
using VariableTestParam3 = std::integral_constant<int, 2>; // 1024 buffer, 4 channels, 5 inputs
using VariableTestParam4 = std::integral_constant<int, 3>; // 256 buffer, 1 channel, 7 inputs
using VariableTestParam5 = std::integral_constant<int, 4>; // 512 buffer, 2 channels, 2 inputs
using VariableTestParam6 = std::integral_constant<int, 5>; // 1024 buffer, 4 channels, 4 inputs

constexpr VariableInputTestParams get_variable_test_params(int index) {
    constexpr VariableInputTestParams params[] = {
        {256, 1, 1, "256_buffer_1_channel_1_input"},
        {512, 2, 3, "512_buffer_2_channels_3_inputs"},
        {1024, 4, 5, "1024_buffer_4_channels_5_inputs"},
        {256, 1, 7, "256_buffer_1_channel_7_inputs"},
        {512, 2, 2, "512_buffer_2_channels_2_inputs"},
        {1024, 4, 4, "1024_buffer_4_channels_4_inputs"}
    };
    return params[index];
}

TEMPLATE_TEST_CASE("AudioMultitrackJoinRenderStage - Basic Join Test", 
                   "[audio_multitrack_join_render_stage][gl_test][template]", 
                   TestParam1, TestParam2, TestParam3) {
    
    // Get test parameters for this template instantiation
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr float GEN1_CONSTANT = 0.3f;
    constexpr float GEN2_CONSTANT = 0.4f;
    constexpr float EXPECTED_OUTPUT = GEN1_CONSTANT + GEN2_CONSTANT;
    constexpr int NUM_FRAMES = 5; // Short test

    // Initialize window and OpenGL context
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    // Create constant generator shaders
    std::string constant_shader = R"(
void main() {
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    output_audio_texture = vec4(1.0) + stream_audio; // Will be modified per generator
    debug_audio_texture = output_audio_texture;
}
)";

    // Create two generator render stages with different constants
    std::string gen1_shader = constant_shader;
    size_t pos = gen1_shader.find("vec4(1.0)");
    gen1_shader.replace(pos, 9, "vec4(" + std::to_string(GEN1_CONSTANT) + ")");

    std::string gen2_shader = constant_shader;
    pos = gen2_shader.find("vec4(1.0)");
    gen2_shader.replace(pos, 9, "vec4(" + std::to_string(GEN2_CONSTANT) + ")");

    AudioRenderStage gen1(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen1_shader, true);
    AudioRenderStage gen2(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen2_shader, true);

    // Create multitrack join with 2 tracks
    AudioMultitrackJoinRenderStage join(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 2);

    // Create final render stage
    AudioFinalRenderStage final_render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Connect generators to join (remove final connection if not testing it)
    REQUIRE(gen1.connect_render_stage(&join));
    REQUIRE(gen2.connect_render_stage(&join));

    // No final connection needed if only testing join

    // Add global_time parameter
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time_param->set_value(0);
    global_time_param->initialize();

    // Initialize all stages
    REQUIRE(gen1.initialize());
    REQUIRE(gen2.initialize());
    REQUIRE(join.initialize());
    // Remove final init and bind

    context.prepare_draw();
    
    // Bind all stages
    REQUIRE(gen1.bind());
    REQUIRE(gen2.bind());
    REQUIRE(join.bind());
    // Remove final bind

    // Render multiple frames
    std::vector<float> first_channel_samples;
    first_channel_samples.reserve(BUFFER_SIZE * NUM_FRAMES);

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        global_time_param->set_value(frame);
        global_time_param->render();

        gen1.render(frame);
        gen2.render(frame);
        
        join.render(frame);
        
        // Get output directly from join
        auto output_param = join.find_parameter("output_audio_texture");
        REQUIRE(output_param != nullptr);

        const float* output_data = static_cast<const float*>(output_param->get_value());
        REQUIRE(output_data != nullptr);

        // Store only first channel samples
        for (int i = 0; i < BUFFER_SIZE; i++) {
            first_channel_samples.push_back(output_data[i]);
        }
    }

    // Verify
    REQUIRE(first_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);

    SECTION("Join Verification") {
        INFO("Testing first channel");
        
        // Check that all samples are approximately the expected output value
        for (size_t i = 0; i < first_channel_samples.size(); ++i) {
            INFO("Sample " << i << " value: " << first_channel_samples[i] << ", expected: " << EXPECTED_OUTPUT);
            REQUIRE(first_channel_samples[i] == Catch::Approx(EXPECTED_OUTPUT).margin(0.01f));
        }
        
        // Test consistency - all samples should be identical
        for (size_t i = 1; i < first_channel_samples.size(); ++i) {
            REQUIRE(first_channel_samples[i] == Catch::Approx(first_channel_samples[0]).margin(0.001f));
        }
    }
}

TEMPLATE_TEST_CASE("AudioMultitrackJoinRenderStage dynamic input switch", "[audio_multitrack_join_render_stage][gl_test][template]",
                   TestParam1, TestParam2, TestParam3) {
    
    // Get test parameters
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_FRAMES = 2; // Frames per phase

    // Initialize window and OpenGL context
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    // Create constant generator shaders with placeholders
    std::string constant_shader = R"(
void main() {
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    output_audio_texture = vec4(PLACEHOLDER) + stream_audio;
    debug_audio_texture = output_audio_texture;
}
)";

    // Create four generators with different constants
    std::vector<float> constants = {0.1f, 0.2f, 0.3f, 0.4f};

    // Generator 0
    std::string gen0_shader = constant_shader;
    size_t pos0 = gen0_shader.find("PLACEHOLDER");
    gen0_shader.replace(pos0, 11, std::to_string(constants[0]));
    AudioRenderStage gen0(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen0_shader, true);

    // Generator 1
    std::string gen1_shader = constant_shader;
    size_t pos1 = gen1_shader.find("PLACEHOLDER");
    gen1_shader.replace(pos1, 11, std::to_string(constants[1]));
    AudioRenderStage gen1(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen1_shader, true);

    // Generator 2 (replacement for gen1)
    std::string gen2_shader = constant_shader;
    size_t pos2 = gen2_shader.find("PLACEHOLDER");
    gen2_shader.replace(pos2, 11, std::to_string(constants[2]));
    AudioRenderStage gen2(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen2_shader, true);

    // Generator 3 (replacement for gen0)
    std::string gen3_shader = constant_shader;
    size_t pos3 = gen3_shader.find("PLACEHOLDER");
    gen3_shader.replace(pos3, 11, std::to_string(constants[3]));
    AudioRenderStage gen3(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen3_shader, true);

    // Create multitrack join with 2 tracks
    AudioMultitrackJoinRenderStage join(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 2);

    // Initialize all stages
    REQUIRE(gen0.initialize());
    REQUIRE(gen1.initialize());
    REQUIRE(gen2.initialize());
    REQUIRE(gen3.initialize());
    REQUIRE(join.initialize());

    // Initially connect gen0 and gen1
    REQUIRE(gen0.connect_render_stage(&join));
    REQUIRE(gen1.connect_render_stage(&join));

    context.prepare_draw();
    
    REQUIRE(gen0.bind());
    REQUIRE(gen1.bind());
    REQUIRE(join.bind());

    // Render initial frames and verify sum
    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        gen0.render(frame);
        gen1.render(frame);
        join.render(frame);
        
        auto output_param = join.find_parameter("output_audio_texture");
        REQUIRE(output_param != nullptr);
        const float* output_data = static_cast<const float*>(output_param->get_value());
        REQUIRE(output_data != nullptr);

        float expected = constants[0] + constants[1];
        for (int i = 0; i < BUFFER_SIZE; ++i) {
            REQUIRE(output_data[i] == Catch::Approx(expected).margin(0.01f));
        }
    }

    // Disconnect gen1 and connect gen2
    REQUIRE(gen1.disconnect_render_stage(&join));
    REQUIRE(gen2.connect_render_stage(&join));
    REQUIRE(gen2.bind());

    // Render more frames and verify new sum without carryover from gen1
    for (int frame = NUM_FRAMES; frame < 2 * NUM_FRAMES; frame++) {
        gen0.render(frame);
        gen2.render(frame);
        join.render(frame);
        
        auto output_param = join.find_parameter("output_audio_texture");
        const float* output_data = static_cast<const float*>(output_param->get_value());

        float expected = constants[0] + constants[2];
        float old_expected = constants[0] + constants[1]; // For no carryover check
        
        for (int i = 0; i < BUFFER_SIZE; ++i) {
            float actual = output_data[i];
            REQUIRE(actual == Catch::Approx(expected).margin(0.01f));
            REQUIRE_FALSE(actual == Catch::Approx(old_expected).margin(0.01f));
        }
    }

    // Disconnect gen0 and connect gen3
    REQUIRE(gen0.disconnect_render_stage(&join));
    REQUIRE(gen3.connect_render_stage(&join));
    REQUIRE(gen3.bind());

    // Render final frames and verify new sum without carryover from gen0
    for (int frame = 2 * NUM_FRAMES; frame < 3 * NUM_FRAMES; frame++) {
        gen3.render(frame);
        gen2.render(frame);
        join.render(frame);
        
        auto output_param = join.find_parameter("output_audio_texture");
        const float* output_data = static_cast<const float*>(output_param->get_value());

        float expected = constants[3] + constants[2];
        float old_expected = constants[0] + constants[2]; // For no carryover check
        
        for (int i = 0; i < BUFFER_SIZE; ++i) {
            float actual = output_data[i];
            REQUIRE(actual == Catch::Approx(expected).margin(0.01f));
            REQUIRE_FALSE(actual == Catch::Approx(old_expected).margin(0.01f));
        }
    }

    // Cleanup
    gen3.unbind();
    gen2.unbind();
    join.unbind();
}

TEMPLATE_TEST_CASE("AudioMultitrackJoinRenderStage - Variable Inputs Test", 
                   "[audio_multitrack_join_render_stage][gl_test][template]", 
                   VariableTestParam1, VariableTestParam2, VariableTestParam3, VariableTestParam4, VariableTestParam5, VariableTestParam6) {
    
    // Get test parameters for this template instantiation
    constexpr auto params = get_variable_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int NUM_INPUTS = params.num_inputs;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_FRAMES = 3; // Short test

    // Initialize window and OpenGL context
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    // Create constant generator shader template
    std::string constant_shader = R"(
void main() {
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    output_audio_texture = vec4(1.0) + stream_audio; // Will be modified per generator
    debug_audio_texture = output_audio_texture;
}
)";

    // Create generator render stages with different constants
    std::vector<std::unique_ptr<AudioRenderStage>> generators;
    std::vector<std::string> constant_strings;
    std::vector<float> constants;
    float expected_sum = 0.0f;
    
    // Generate constants based on number of inputs
    for (int i = 0; i < NUM_INPUTS; ++i) {
        float constant = 0.1f + (i * 0.1f);
        constants.push_back(constant);
        constant_strings.push_back(std::to_string(constant));
        expected_sum += constant;
    }

    for (int i = 0; i < NUM_INPUTS; ++i) {
        std::string gen_shader = constant_shader;
        // Replace the placeholder with the constant value
        size_t pos = gen_shader.find("vec4(1.0)");
        gen_shader.replace(pos, 9, "vec4(" + constant_strings[i] + ")");
        
        generators.emplace_back(std::make_unique<AudioRenderStage>(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen_shader, true));
    }

    // Create multitrack join with 8 tracks
    AudioMultitrackJoinRenderStage join(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, NUM_INPUTS);

    // Initialize all stages
    for (auto& gen : generators) {
        REQUIRE(gen->initialize());
    }
    REQUIRE(join.initialize());

    // Connect all generators to join
    for (auto& gen : generators) {
        REQUIRE(gen->connect_render_stage(&join));
    }

    context.prepare_draw();
    
    // Bind all stages
    for (auto& gen : generators) {
        REQUIRE(gen->bind());
    }
    REQUIRE(join.bind());

    // Render multiple frames
    std::vector<float> first_channel_samples;
    first_channel_samples.reserve(BUFFER_SIZE * NUM_FRAMES);

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        // Render all generators
        for (auto& gen : generators) {
            gen->render(frame);
        }
        
        join.render(frame);
        
        // Get output directly from join
        auto output_param = join.find_parameter("output_audio_texture");
        REQUIRE(output_param != nullptr);

        const float* output_data = static_cast<const float*>(output_param->get_value());
        REQUIRE(output_data != nullptr);

        // Store only first channel samples
        for (int i = 0; i < BUFFER_SIZE; i++) {
            first_channel_samples.push_back(output_data[i]);
        }
    }

    // Verify
    REQUIRE(first_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);

    SECTION("Variable Inputs Join Verification") {
        INFO("Testing first channel with " << NUM_INPUTS << " inputs");
        
        // Check that all samples are approximately the expected output value
        for (size_t i = 0; i < first_channel_samples.size(); ++i) {
            INFO("Sample " << i << " value: " << first_channel_samples[i] << ", expected: " << expected_sum);
            REQUIRE(first_channel_samples[i] == Catch::Approx(expected_sum).margin(0.01f));
        }
        
        // Test consistency - all samples should be identical
        for (size_t i = 1; i < first_channel_samples.size(); ++i) {
            REQUIRE(first_channel_samples[i] == Catch::Approx(first_channel_samples[0]).margin(0.001f));
        }
    }

    // Cleanup
    for (auto& gen : generators) {
        gen->unbind();
    }
    join.unbind();
}
