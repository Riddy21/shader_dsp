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
constexpr int WIDTH = 256;
constexpr int HEIGHT = 1;
TEST_CASE("AudioRenderStage with OpenGL context", "[audio_render_stage][gl_test]") {
    SDLWindow window(WIDTH, HEIGHT);

    GLContext context;

    SECTION("Basic render stage initialization and rendering") {
        constexpr int BUFFER_SIZE = 256;
        constexpr int SAMPLE_RATE = 44100;
        constexpr int NUM_CHANNELS = 2;

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
        
        // Get output datauu
        auto output_param = render_stage.find_parameter("output_audio_texture");
        REQUIRE(output_param != nullptr);

        const float* output_data = static_cast<const float*>(output_param->get_value());

        REQUIRE(output_data != nullptr);

        // Verify output contains simple sine wave data
        for (int i = 0; i < BUFFER_SIZE; ++i) {
            // Calculate expected sine wave based on our simple shader logic
            float tex_coord_x = (static_cast<float>(i) + 0.5f) / BUFFER_SIZE; // TexCoord.x for this pixel
            float sample_pos = tex_coord_x * BUFFER_SIZE;
            float time_in_seconds = sample_pos / SAMPLE_RATE;
            float expected = sin(2.0f * M_PI * time_in_seconds);
            
            float actual = output_data[i]; // Single channel
            REQUIRE(actual == Catch::Approx(expected).margin(0.1f));
        }

        auto debug_param = render_stage.find_parameter("debug_audio_texture");
        REQUIRE(debug_param != nullptr);
        const float* debug_data = static_cast<const float*>(debug_param->get_value());
        REQUIRE(debug_data != nullptr);
        for (int i = 0; i < BUFFER_SIZE; ++i) {
            REQUIRE(debug_data[i] == Catch::Approx(float(BUFFER_SIZE) / 1000.0f).margin(0.1f));
        }

        render_stage.unbind();
    }
}

TEST_CASE("AudioRenderStage pass-through chain", "[audio_render_stage][gl_test]") {
    SDLWindow window(WIDTH, HEIGHT);
    GLContext context;

    constexpr int BUFFER_SIZE = 256;
    constexpr int SAMPLE_RATE = 44100;
    constexpr int NUM_CHANNELS = 2;

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

    stage1.render(0);

    // Validate debug texture of stage1 is sine wave
    auto debug_param = stage1.find_parameter("output_audio_texture");
    REQUIRE(debug_param != nullptr);
    const float *debug_data = static_cast<const float *>(debug_param->get_value());
    REQUIRE(debug_data != nullptr);
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        float tex_coord_x = (static_cast<float>(i) + 0.5f) / BUFFER_SIZE; // TexCoord.x for this pixel
        float sample_pos = tex_coord_x * BUFFER_SIZE;
        float time_in_seconds = sample_pos / SAMPLE_RATE;
        float expected = sin(2.0f * M_PI * time_in_seconds);
        REQUIRE(debug_data[i] == Catch::Approx(expected).margin(0.1f));
    }
    
    REQUIRE(stage2.bind());
    
    stage2.render(0);

    // Validate data passed through correctly
    auto output_param = stage2.find_parameter("output_audio_texture");
    REQUIRE(output_param != nullptr);
    const float *output_data = static_cast<const float *>(output_param->get_value());
    REQUIRE(output_data != nullptr);

    for (int i = 0; i < BUFFER_SIZE; ++i) {
        float tex_coord_x = (static_cast<float>(i) + 0.5f) / BUFFER_SIZE; // TexCoord.x for this pixel
        float sample_pos = tex_coord_x * BUFFER_SIZE;
        float time_in_seconds = sample_pos / SAMPLE_RATE;
        float expected = sin(2.0f * M_PI * time_in_seconds);
        REQUIRE(output_data[i] == Catch::Approx(expected).margin(0.1f));
    }

    // Debug texture of stage2 should remain zeros
    debug_param = stage2.find_parameter("debug_audio_texture");
    REQUIRE(debug_param != nullptr);
    debug_data = static_cast<const float *>(debug_param->get_value());
    REQUIRE(debug_data != nullptr);
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        REQUIRE(debug_data[i] == Catch::Approx(0.0f).margin(0.1f));
    }

    stage2.unbind();
    stage1.unbind();
}