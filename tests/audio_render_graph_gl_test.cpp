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

