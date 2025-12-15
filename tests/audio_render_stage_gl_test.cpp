#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"
#include "utilities/egl_compatibility.h"

#include "audio_core/audio_render_stage.h"
#include "audio_core/audio_render_graph.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_render_stage/audio_effect_render_stage.h"
#include "audio_parameter/audio_texture2d_parameter.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_output/audio_player_output.h"
#include "framework/test_main.h"
#include "framework/csv_test_output.h"

#include <functional>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <sstream>

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

        // Prepare audio output (only if enabled)
        AudioPlayerOutput* audio_output = nullptr;
        if (is_audio_output_enabled()) {
            audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
            REQUIRE(audio_output->open());
            REQUIRE(audio_output->start());
        }

        std::vector<float> output_samples;
        output_samples.reserve(BUFFER_SIZE * NUM_CHANNELS);

        render_stage.render(0);
        
        // Get output data
        auto output_param = render_stage.find_parameter("output_audio_texture");
        REQUIRE(output_param != nullptr);

        const float* output_data = static_cast<const float*>(output_param->get_value());

        REQUIRE(output_data != nullptr);

        // Store output samples (channel-major format)
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            const float* channel_data = output_data + (ch * BUFFER_SIZE);
            for (int i = 0; i < BUFFER_SIZE; ++i) {
                output_samples.push_back(channel_data[i]);
            }
        }

        // Push to audio output (only if enabled)
        if (audio_output) {
            // Convert channel-major to interleaved for audio output
            std::vector<float> interleaved(BUFFER_SIZE * NUM_CHANNELS);
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                const float* channel_data = output_data + (ch * BUFFER_SIZE);
                for (int i = 0; i < BUFFER_SIZE; ++i) {
                    interleaved[i * NUM_CHANNELS + ch] = channel_data[i];
                }
            }
            while (!audio_output->is_ready()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            audio_output->push(interleaved.data());
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            audio_output->stop();
            audio_output->close();
            delete audio_output;
        }

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

        // CSV output (only if enabled)
        if (is_csv_output_enabled()) {
            std::string csv_output_dir = "build/tests/csv_output";
            system(("mkdir -p " + csv_output_dir).c_str());
            
            // Convert to per-channel format
            std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                output_samples_per_channel[ch].reserve(BUFFER_SIZE);
            }
            
            // Convert from channel-major to per-channel vectors
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                const float* channel_data = output_samples.data() + (ch * BUFFER_SIZE);
                for (int i = 0; i < BUFFER_SIZE; ++i) {
                    output_samples_per_channel[ch].push_back(channel_data[i]);
                }
            }
            
            std::ostringstream filename_stream;
            filename_stream << csv_output_dir << "/audio_render_stage_sine_wave_" << params.name << ".csv";
            std::string filename = filename_stream.str();
            
            CSVTestOutput csv_writer(filename, SAMPLE_RATE);
            REQUIRE(csv_writer.is_open());
            csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
            csv_writer.close();
            
            std::cout << "Wrote output samples to " << filename << " (" 
                      << BUFFER_SIZE << " samples, " << NUM_CHANNELS << " channels)" << std::endl;
        }
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

    // Prepare audio output (only if enabled)
    AudioPlayerOutput* audio_output = nullptr;
    if (is_audio_output_enabled()) {
        audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
        REQUIRE(audio_output->open());
        REQUIRE(audio_output->start());
    }

    std::vector<float> stage1_samples, stage2_samples;
    stage1_samples.reserve(BUFFER_SIZE * NUM_CHANNELS);
    stage2_samples.reserve(BUFFER_SIZE * NUM_CHANNELS);

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

    // Store stage2 output samples (channel-major format)
    for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
        const float* channel_data = output_data + (ch * BUFFER_SIZE);
        for (int i = 0; i < BUFFER_SIZE; ++i) {
            stage2_samples.push_back(channel_data[i]);
        }
    }

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

    // Push to audio output (only if enabled)
    if (audio_output) {
        // Convert channel-major to interleaved for audio output
        std::vector<float> interleaved(BUFFER_SIZE * NUM_CHANNELS);
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            const float* channel_data = output_data + (ch * BUFFER_SIZE);
            for (int i = 0; i < BUFFER_SIZE; ++i) {
                interleaved[i * NUM_CHANNELS + ch] = channel_data[i];
            }
        }
        while (!audio_output->is_ready()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        audio_output->push(interleaved.data());
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        audio_output->stop();
        audio_output->close();
        delete audio_output;
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

    // CSV output (only if enabled)
    if (is_csv_output_enabled()) {
        std::string csv_output_dir = "build/tests/csv_output";
        system(("mkdir -p " + csv_output_dir).c_str());
        
        // Convert to per-channel format
        std::vector<std::vector<float>> stage2_samples_per_channel(NUM_CHANNELS);
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            stage2_samples_per_channel[ch].reserve(BUFFER_SIZE);
        }
        
        // Convert from channel-major to per-channel vectors
        for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
            const float* channel_data = stage2_samples.data() + (ch * BUFFER_SIZE);
            for (int i = 0; i < BUFFER_SIZE; ++i) {
                stage2_samples_per_channel[ch].push_back(channel_data[i]);
            }
        }
        
        std::ostringstream filename_stream;
        filename_stream << csv_output_dir << "/audio_render_stage_passthrough_" << params.name << ".csv";
        std::string filename = filename_stream.str();
        
        CSVTestOutput csv_writer(filename, SAMPLE_RATE);
        REQUIRE(csv_writer.is_open());
        csv_writer.write_channels(stage2_samples_per_channel, SAMPLE_RATE);
        csv_writer.close();
        
        std::cout << "Wrote passthrough output samples to " << filename << " (" 
                  << BUFFER_SIZE << " samples, " << NUM_CHANNELS << " channels)" << std::endl;
    }
}

TEMPLATE_TEST_CASE("AudioRenderStage dynamic passthrough switch", "[audio_render_stage][gl_test][template]",
                   TestParam1, TestParam2, TestParam3) {
    
    // Get test parameters for this template instantiation
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;

    // Initialize window and OpenGL context with appropriate dimensions
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    // --- Stage 1: produce time-dependent sine wave ---
    const char *stage1_shader_path = "build/tests/test_dynamic_render_stage1_frag.glsl";
    {
        std::ofstream fs(stage1_shader_path);
        fs << R"(
uniform float time;
void main(){
    float sample_pos = TexCoord.x * float(buffer_size);
    float time_in_seconds = sample_pos / float(sample_rate);
    float sine_wave = sin(TWO_PI * (time_in_seconds + time));

    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    
    // Output the sine wave
    output_audio_texture = vec4(sine_wave) + stream_audio;
    debug_audio_texture  = vec4(sine_wave) + stream_audio;
}
)";
    }

    // --- Passthrough stages ---
    const char *passthrough_shader_path = "build/tests/test_dynamic_render_stage2_frag.glsl";
    {
        std::ofstream fs(passthrough_shader_path);
        fs << R"(
void main(){
    vec4 v = texture(stream_audio_texture, TexCoord);
    output_audio_texture = v;
    debug_audio_texture  = vec4(0.0);
}
)";
    }

    AudioRenderStage stage1(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, stage1_shader_path);
    AudioRenderStage passthrough1(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, passthrough_shader_path);
    AudioRenderStage passthrough2(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, passthrough_shader_path);

    REQUIRE(stage1.initialize());
    REQUIRE(passthrough1.initialize());
    REQUIRE(passthrough2.initialize());

    REQUIRE(stage1.connect_render_stage(&passthrough1));

    context.prepare_draw();

    REQUIRE(stage1.bind());
    REQUIRE(passthrough1.bind());

    // Render first 2 frames with passthrough1
    for (int t = 0; t < 2; ++t) {
        // Set time uniform
        glUseProgram(stage1.get_shader_program());
        glUniform1f(glGetUniformLocation(stage1.get_shader_program(), "time"), float(t));

        stage1.render(0);
        passthrough1.render(0);

        // Validate output
        auto output_param = passthrough1.find_parameter("output_audio_texture");
        REQUIRE(output_param != nullptr);
        const float* output_data = static_cast<const float*>(output_param->get_value());
        REQUIRE(output_data != nullptr);

        for (int i = 0; i < BUFFER_SIZE; ++i) {
            float tex_coord_x = (static_cast<float>(i) + 0.5f) / BUFFER_SIZE;
            float sample_pos = tex_coord_x * BUFFER_SIZE;
            float time_in_seconds = sample_pos / SAMPLE_RATE;
            float expected = sin(2.0f * M_PI * (time_in_seconds + t));
            
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                REQUIRE(output_data[i + ch * BUFFER_SIZE] == Catch::Approx(expected).margin(0.1f));
            }
        }
    }

    // Disconnect and connect to passthrough2
    REQUIRE(stage1.disconnect_render_stage(&passthrough1));
    REQUIRE(stage1.connect_render_stage(&passthrough2));
    REQUIRE(stage1.bind());
    REQUIRE(passthrough2.bind());

    // Render next 2 frames with passthrough2 and check continuity
    for (int t = 2; t < 4; ++t) {
        glUseProgram(stage1.get_shader_program());
        glUniform1f(glGetUniformLocation(stage1.get_shader_program(), "time"), float(t));

        stage1.render(0);
        passthrough2.render(0);

        auto output_param = passthrough2.find_parameter("output_audio_texture");
        REQUIRE(output_param != nullptr);
        const float* output_data = static_cast<const float*>(output_param->get_value());
        REQUIRE(output_data != nullptr);

        for (int i = 0; i < BUFFER_SIZE; ++i) {
            float tex_coord_x = (static_cast<float>(i) + 0.5f) / BUFFER_SIZE;
            float sample_pos = tex_coord_x * BUFFER_SIZE;
            float time_in_seconds = sample_pos / SAMPLE_RATE;
            float expected = sin(2.0f * M_PI * (time_in_seconds + t));
            
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                REQUIRE(output_data[i + ch * BUFFER_SIZE] == Catch::Approx(expected).margin(0.1f));
            }
        }
    }

    passthrough2.unbind();
    stage1.unbind();
}

TEMPLATE_TEST_CASE("AudioRenderStage dynamic generator switch", "[audio_render_stage][gl_test][template]",
                   TestParam1, TestParam2, TestParam3) {
    
    // Get test parameters for this template instantiation
    constexpr auto params = get_test_params(TestType::value);
    constexpr int BUFFER_SIZE = params.buffer_size;
    constexpr int NUM_CHANNELS = params.num_channels;
    constexpr int SAMPLE_RATE = 44100;

    // Initialize window and OpenGL context with appropriate dimensions
    SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
    GLContext context;

    // --- Passthrough stage ---
    const char *passthrough_shader_path = "build/tests/test_dynamic_generator_passthrough_frag.glsl";
    {
        std::ofstream fs(passthrough_shader_path);
        fs << R"(
void main(){
    vec4 v = texture(stream_audio_texture, TexCoord);
    output_audio_texture = v;
    debug_audio_texture  = vec4(0.0);
}
)";
    }

    AudioRenderStage passthrough(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, passthrough_shader_path);

    // --- Generator stages with different constants ---
    std::vector<float> constants = {0.1f, 0.2f, 0.3f}; // Distinct constants for each generator

    // Generator 0
    const char *gen0_shader_path = "build/tests/test_dynamic_generator_frag0.glsl";
    {
        std::ofstream fs(gen0_shader_path);
        fs << R"(
uniform float time;
void main(){
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    
    output_audio_texture = vec4(0.1) + stream_audio;
    debug_audio_texture  = output_audio_texture;
}
)";
    }
    AudioRenderStage gen0(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen0_shader_path);

    // Generator 1
    const char *gen1_shader_path = "build/tests/test_dynamic_generator_frag1.glsl";
    {
        std::ofstream fs(gen1_shader_path);
        fs << R"(
uniform float time;
void main(){
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    
    output_audio_texture = vec4(0.2) + stream_audio;
    debug_audio_texture  = output_audio_texture;
}
)";
    }
    AudioRenderStage gen1(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen1_shader_path);

    // Generator 2
    const char *gen2_shader_path = "build/tests/test_dynamic_generator_frag2.glsl";
    {
        std::ofstream fs(gen2_shader_path);
        fs << R"(
uniform float time;
void main(){
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    
    output_audio_texture = vec4(0.3) + stream_audio;
    debug_audio_texture  = output_audio_texture;
}
)";
    }
    AudioRenderStage gen2(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, gen2_shader_path);

    std::vector<AudioRenderStage*> generators = {&gen0, &gen1, &gen2};

    REQUIRE(passthrough.initialize());
    REQUIRE(gen0.initialize());
    REQUIRE(gen1.initialize());
    REQUIRE(gen2.initialize());

    // Initially connect generator 0 to passthrough
    REQUIRE(generators[0]->connect_render_stage(&passthrough));

    context.prepare_draw();

    REQUIRE(generators[0]->bind());
    REQUIRE(passthrough.bind());

    // Render 2 frames with generator 0
    for (int t = 0; t < 2; ++t) {
        // Set time uniform (even if not used, for consistency)
        glUseProgram(generators[0]->get_shader_program());
        glUniform1f(glGetUniformLocation(generators[0]->get_shader_program(), "time"), float(t));

        generators[0]->render(0);
        passthrough.render(0);

        // Validate output
        auto output_param = passthrough.find_parameter("output_audio_texture");
        REQUIRE(output_param != nullptr);
        const float* output_data = static_cast<const float*>(output_param->get_value());
        REQUIRE(output_data != nullptr);

        for (int i = 0; i < BUFFER_SIZE; ++i) {
            float expected = constants[0];
            
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                REQUIRE(output_data[i + ch * BUFFER_SIZE] == Catch::Approx(expected).margin(0.01f));
            }
        }
    }

    // Switch to generator 1
    REQUIRE(generators[0]->disconnect_render_stage(&passthrough));
    REQUIRE(generators[1]->connect_render_stage(&passthrough));
    REQUIRE(generators[1]->bind()); // Bind new generator

    // Render 2 frames with generator 1, check no carryover from gen0
    for (int t = 2; t < 4; ++t) {
        glUseProgram(generators[1]->get_shader_program());
        glUniform1f(glGetUniformLocation(generators[1]->get_shader_program(), "time"), float(t));

        generators[1]->render(0);
        passthrough.render(0);

        auto output_param = passthrough.find_parameter("output_audio_texture");
        const float* output_data = static_cast<const float*>(output_param->get_value());

        for (int i = 0; i < BUFFER_SIZE; ++i) {
            float expected = constants[1];
            float old_expected = constants[0]; // To ensure not matching old

            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                float actual = output_data[i + ch * BUFFER_SIZE];
                REQUIRE(actual == Catch::Approx(expected).margin(0.01f));
                REQUIRE_FALSE(actual == Catch::Approx(old_expected).margin(0.01f)); // No carryover
            }
        }
    }

    // Switch to generator 2
    REQUIRE(generators[1]->disconnect_render_stage(&passthrough));
    REQUIRE(generators[2]->connect_render_stage(&passthrough));
    REQUIRE(generators[2]->bind());

    // Render 2 frames with generator 2, check no carryover from gen1
    for (int t = 4; t < 6; ++t) {
        glUseProgram(generators[2]->get_shader_program());
        glUniform1f(glGetUniformLocation(generators[2]->get_shader_program(), "time"), float(t));

        generators[2]->render(0);
        passthrough.render(0);

        auto output_param = passthrough.find_parameter("output_audio_texture");
        const float* output_data = static_cast<const float*>(output_param->get_value());

        for (int i = 0; i < BUFFER_SIZE; ++i) {
            float expected = constants[2];
            float old_expected = constants[1];

            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                float actual = output_data[i + ch * BUFFER_SIZE];
                REQUIRE(actual == Catch::Approx(expected).margin(0.01f));
                REQUIRE_FALSE(actual == Catch::Approx(old_expected).margin(0.01f));
            }
        }
    }

    generators[2]->unbind();
    passthrough.unbind();
}