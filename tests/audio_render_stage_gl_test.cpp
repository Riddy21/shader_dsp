#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"

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
    float sine_wave = sin(TWO_PI * 440.0 * time_in_seconds);

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
        std::ofstream shader_file("build/shaders/test_render_stage_frag.glsl");
        shader_file << test_frag_shader;
        shader_file.close();

        // Create render stage with custom fragment shader
        AudioRenderStage render_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, 
                                     "build/shaders/test_render_stage_frag.glsl");
        
        // Debug: Print the combined shader source
        std::cout << "Combined fragment shader source:" << std::endl;
        std::cout << render_stage.m_fragment_shader_source << std::endl;
        
        // Initialize the render stage
        REQUIRE(render_stage.initialize());

        // Debug: Check if initialization was successful
        std::cout << "Render stage initialized successfully" << std::endl;
        std::cout << "Is initialized: " << (render_stage.is_initialized() ? "true" : "false") << std::endl;

        // Check OpenGL error after initialization
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cout << "OpenGL error after initialize: 0x" << std::hex << error << std::dec << std::endl;
        }

        // Bind and render
        REQUIRE(render_stage.bind());
        std::cout << "Render stage bound successfully" << std::endl;

        // Check framebuffer completeness after binding
        GLint fbo = render_stage.get_framebuffer();
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        GLenum fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (fb_status != GL_FRAMEBUFFER_COMPLETE) {
            std::cout << "Framebuffer incomplete after bind: 0x" << std::hex << fb_status << std::dec << std::endl;
        } else {
            std::cout << "Framebuffer is complete after bind" << std::endl;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Check OpenGL error after binding
        error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cout << "OpenGL error after bind: 0x" << std::hex << error << std::dec << std::endl;
        }

        render_stage.render(0);
        std::cout << "Render stage rendered successfully" << std::endl;

        // Check framebuffer completeness after rendering
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (fb_status != GL_FRAMEBUFFER_COMPLETE) {
            std::cout << "Framebuffer incomplete after render: 0x" << std::hex << fb_status << std::dec << std::endl;
        } else {
            std::cout << "Framebuffer is complete after render" << std::endl;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Check OpenGL error after rendering
        error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cout << "OpenGL error after render: 0x" << std::hex << error << std::dec << std::endl;
        }

        // Get output data
        auto output_param = render_stage.find_parameter("output_audio_texture");
        REQUIRE(output_param != nullptr);

        const float* output_data = static_cast<const float*>(output_param->get_value());

        REQUIRE(output_data != nullptr);


        // Debug: Print first few values to see what we're getting
        std::cout << "First 10 output values: ";
        for (int i = 0; i < 10; ++i) {
            std::cout << output_data[i] << " ";
        }
        std::cout << std::endl;

        // Debug: Check if the shader compiled successfully
        GLuint program = render_stage.get_shader_program();
        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            GLchar info_log[512];
            glGetProgramInfoLog(program, 512, NULL, info_log);
            std::cout << "Shader program linking failed: " << info_log << std::endl;
        } else {
            std::cout << "Shader program linked successfully" << std::endl;
        }

        // Verify output contains simple sine wave data
        for (int i = 0; i < BUFFER_SIZE; ++i) {
            // Calculate expected sine wave based on our simple shader logic
            float time_in_seconds = static_cast<float>(i) / SAMPLE_RATE;
            float expected = sin(2.0f * M_PI * 440.0f * time_in_seconds);
            
            float actual = output_data[i * 4]; // Red channel
            REQUIRE(actual == Catch::Approx(expected).margin(0.1f));
        }

        render_stage.unbind();
    }
}