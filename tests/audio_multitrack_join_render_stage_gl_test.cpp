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
#include <type_traits>

// Test parameter structure to hold buffer size and channel count combinations
struct TestParams {
    int buffer_size;
    int num_channels;
    const char* name;
};

// Update parameters to focus on single channel
using TestParam1 = std::integral_constant<int, 0>; // 256 buffer, 1 channel
using TestParam2 = std::integral_constant<int, 1>; // 512 buffer, 1 channel
using TestParam3 = std::integral_constant<int, 2>; // 1024 buffer, 1 channel

constexpr TestParams get_test_params(int index) {
    constexpr TestParams params[] = {
        {256, 1, "256_buffer_1_channel"},
        {512, 1, "512_buffer_1_channel"},
        {1024, 1, "1024_buffer_1_channel"}
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
    std::vector<float> left_channel_samples;
    std::vector<float> right_channel_samples;
    left_channel_samples.reserve(BUFFER_SIZE * NUM_FRAMES);
    if (NUM_CHANNELS > 1) right_channel_samples.reserve(BUFFER_SIZE * NUM_FRAMES);

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

        // Since NUM_CHANNELS=1, store samples
        for (int i = 0; i < BUFFER_SIZE; i++) {
            left_channel_samples.push_back(output_data[i]);
        }
    }

    // Verify
    REQUIRE(left_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);
    if (NUM_CHANNELS > 1) REQUIRE(right_channel_samples.size() == BUFFER_SIZE * NUM_FRAMES);

    SECTION("Join Verification") {
        // Test available channels
        std::vector<std::vector<float>*> channels = {&left_channel_samples};
        std::vector<std::string> channel_names = {"Channel_0"};
        
        if (NUM_CHANNELS > 1) {
            channels.push_back(&right_channel_samples);
            channel_names.push_back("Channel_1");
        }
        
        for (size_t ch = 0; ch < channels.size(); ch++) {
            const auto& samples = *channels[ch];
            const std::string& name = channel_names[ch];
            
            INFO("Testing " << name << " channel");
            
            // Check that all samples are approximately the expected output value
            for (size_t i = 0; i < samples.size(); ++i) {
                INFO("Sample " << i << " value: " << samples[i] << ", expected: " << EXPECTED_OUTPUT);
                REQUIRE(samples[i] == Catch::Approx(EXPECTED_OUTPUT).margin(0.01f));
            }
            
            // Test consistency - all samples should be identical
            for (size_t i = 1; i < samples.size(); ++i) {
                REQUIRE(samples[i] == Catch::Approx(samples[0]).margin(0.001f));
            }
        }
    }
}
