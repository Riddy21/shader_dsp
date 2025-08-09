#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"
#include "utilities/egl_compatibility.h"

#include "audio_core/audio_render_stage.h"
#include "audio_core/audio_render_graph.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_render_stage/audio_effect_render_stage.h"
#include "audio_parameter/audio_texture2d_parameter.h"
#include "audio_parameter/audio_uniform_parameter.h"

#include <functional>
#include <cmath>
#include <fstream>
#include <iostream>

/**
 * @brief Tests for render stage functionality with OpenGL context
 * 
 * These tests check the render stage creation, initialization, and rendering in an OpenGL context.
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
        {256, 1, "256_buffer_1_channels"},
        {512, 2, "512_buffer_2_channels"},
        {1024, 3, "1024_buffer_3_channels"}
    };
    return params[index];
}

TEMPLATE_TEST_CASE("AudioRenderStage with OpenGL context", "[audio_render_stage][gl_test][template]",
                   TestParam1, TestParam2, TestParam3) {
    
    // Get test parameters for this template instantiation
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;

    // Initialize window and OpenGL context with appropriate dimensions
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    SECTION("Basic render stage initialization and rendering") {

        // Create test fragment shader as a string
        // This shader simply uses buffer_size, sample_rate, and num_channels to generate basic output
        std::string test_frag_shader = R"(
void main() {
    // Use buffer_size to create a simple pattern
    float sample_pos = TexCoord.x * float(buffer_size);
    float channel_pos = TexCoord.y * float(num_channels);
    
    // Create a simple sine wave using sample_rate
    float time_in_seconds = sample_pos / float(sample_rate);
    float sine_wave = sin(TWO_PI * time_in_seconds);

    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    
    // Output the sine wave
    output_audio_texture = vec4(sine_wave, sine_wave, sine_wave, 1.0) + stream_audio;
    
    // Debug output shows the parameters
    debug_audio_texture = vec4(
        float(buffer_size) / 1000.0,  // Normalized buffer size
        float(sample_rate) / 48000.0, // Normalized sample rate  
        float(num_channels) / 8.0,    // Normalized channel count
        1.0
    );
}
)";

        // Write the shader to a file in build directory so you can see it
        std::ofstream shader_file("build/tests/test_render_stage_frag.glsl");
        shader_file << test_frag_shader;
        shader_file.close();

        // Create render stage with custom fragment shader
        AudioRenderStage render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 
                                     "build/tests/test_render_stage_frag.glsl");
        
        // Initialize the render stage
        REQUIRE(render_stage.initialize());

        // Bind and render
        REQUIRE(render_stage.bind());

        context.prepare_draw();

        render_stage.render(0);
        
        // Get output data
        auto output_param = render_stage.find_parameter("output_audio_texture");
        REQUIRE(output_param != nullptr);

        const float* output_data = static_cast<const float*>(output_param->get_value());

        REQUIRE(output_data != nullptr);

        // Verify output contains simple sine wave data for all channels
        // Each channel occupies BUFFER_SIZE consecutive indices: ch0[0...BUFFER_SIZE-1], ch1[BUFFER_SIZE...2*BUFFER_SIZE-1], etc.
        for (int i = 0; i < BUFFER_SIZE; i++) {
            // Calculate expected sine wave based on our simple shader logic
            float tex_coord_x = (static_cast<float>(i) + 0.5f) / BUFFER_SIZE; // TexCoord.x for this pixel
            float sample_pos = tex_coord_x * BUFFER_SIZE;
            float time_in_seconds = sample_pos / SAMPLE_RATE;
            float expected = sin(2.0f * M_PI * time_in_seconds);
            
            // Test all channels dynamically
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                float actual = output_data[i + ch * BUFFER_SIZE];
                REQUIRE(actual == Catch::Approx(expected).margin(0.1f));
            }
        }

        auto debug_param = render_stage.find_parameter("debug_audio_texture");
        REQUIRE(debug_param != nullptr);
        const float* debug_data = static_cast<const float*>(debug_param->get_value());
        REQUIRE(debug_data != nullptr);
        for (int i = 0; i < BUFFER_SIZE; i++) {
            // Test all channels dynamically
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                REQUIRE(debug_data[i + ch * BUFFER_SIZE] == Catch::Approx(float(BUFFER_SIZE) / 1000.0f).margin(0.1f));
            }
        }

        render_stage.unbind();
    }
}

TEMPLATE_TEST_CASE("AudioRenderStage pass-through chain", "[audio_render_stage][gl_test][template]",
                   TestParam1, TestParam2, TestParam3) {
    
    // Get test parameters for this template instantiation
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;

    // Initialize window and OpenGL context with appropriate dimensions
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    // --- Stage 1: produce sine wave ---
    const char *stage1_shader_path = "build/tests/test_render_stage1_frag.glsl";
    {
        std::ofstream fs(stage1_shader_path);
        fs << R"(
void main(){
    float sample_pos = TexCoord.x * float(buffer_size);
    float time_in_seconds = sample_pos / float(sample_rate);
    float sine_wave = sin(TWO_PI * time_in_seconds);

    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    
    // Output the sine wave
    output_audio_texture = vec4(sine_wave) + stream_audio;
    debug_audio_texture  = vec4(sine_wave) + stream_audio;
}
)";
    }

    // --- Stage 2: pass-through of stream_audio_texture ---
    const char *stage2_shader_path = "build/tests/test_render_stage2_frag.glsl";
    {
        std::ofstream fs(stage2_shader_path);
        fs << R"(
void main(){
    vec4 v = texture(stream_audio_texture, TexCoord);
    output_audio_texture = v;
    debug_audio_texture  = vec4(0.0);
}
)";
    }

    AudioRenderStage stage1(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, stage1_shader_path);
    AudioRenderStage stage2(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, stage2_shader_path);

    REQUIRE(stage1.initialize());
    REQUIRE(stage2.initialize());

    REQUIRE(stage1.connect_render_stage(&stage2));

    context.prepare_draw();

    REQUIRE(stage1.bind());
    REQUIRE(stage2.bind());

    stage1.render(0);

    // Validate debug texture of stage1 is sine wave for all channels
    auto debug_param = stage1.find_parameter("output_audio_texture");
    REQUIRE(debug_param != nullptr);
    const float *debug_data = static_cast<const float *>(debug_param->get_value());
    REQUIRE(debug_data != nullptr);
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        float tex_coord_x = (static_cast<float>(i) + 0.5f) / BUFFER_SIZE; // TexCoord.x for this pixel
        float sample_pos = tex_coord_x * BUFFER_SIZE;
        float time_in_seconds = sample_pos / SAMPLE_RATE;
        float expected = sin(2.0f * M_PI * time_in_seconds);
        
        // Test all channels dynamically
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            REQUIRE(debug_data[i + ch * BUFFER_SIZE] == Catch::Approx(expected).margin(0.1f));
        }
    }
    
    
    stage2.render(0);

    // Validate data passed through correctly for all channels
    auto output_param = stage2.find_parameter("output_audio_texture");
    REQUIRE(output_param != nullptr);
    const float *output_data = static_cast<const float *>(output_param->get_value());
    REQUIRE(output_data != nullptr);

    for (int i = 0; i < BUFFER_SIZE; ++i) {
        float tex_coord_x = (static_cast<float>(i) + 0.5f) / BUFFER_SIZE; // TexCoord.x for this pixel
        float sample_pos = tex_coord_x * BUFFER_SIZE;
        float time_in_seconds = sample_pos / SAMPLE_RATE;
        float expected = sin(2.0f * M_PI * time_in_seconds);
        
        // Test all channels dynamically
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            REQUIRE(output_data[i + ch * BUFFER_SIZE] == Catch::Approx(expected).margin(0.1f));
        }
    }

    // Debug texture of stage2 should remain zeros for all channels
    debug_param = stage2.find_parameter("debug_audio_texture");
    REQUIRE(debug_param != nullptr);
    debug_data = static_cast<const float *>(debug_param->get_value());
    REQUIRE(debug_data != nullptr);
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        // Test all channels dynamically
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            REQUIRE(debug_data[i + ch * BUFFER_SIZE] == Catch::Approx(0.0f).margin(0.1f));
        }
    }

    stage2.unbind();
    stage1.unbind();
}