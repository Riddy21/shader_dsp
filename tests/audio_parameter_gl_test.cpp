#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"

#include "audio_core/audio_parameter.h"
#include "audio_parameter/audio_texture2d_parameter.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"
#include <functional>
#include <cmath>
/**
 * @brief Tests for texture parameter initialization with OpenGL context
 * 
 * These tests check the texture creation and initialization in an OpenGL context.
 * Note: These tests require a valid OpenGL context to run, which may not be available
 * in all test environments. They are marked with [gl] tag.
 */

// Test parameter structure for output tests
struct OutputTestParams {
    int width;
    int height;
    const char* name;
};

// Define 3 different output test parameter combinations
using OutputTestParam1 = std::integral_constant<int, 0>; // 256x1
using OutputTestParam2 = std::integral_constant<int, 1>; // 64x4
using OutputTestParam3 = std::integral_constant<int, 2>; // 128x2

// Parameter lookup function for output tests
constexpr OutputTestParams get_output_test_params(int index) {
    constexpr OutputTestParams params[] = {
        {256, 1, "256x1"},
        {64, 4, "64x4"},
        {128, 2, "128x2"}
    };
    return params[index];
}

TEMPLATE_TEST_CASE("AudioTexture2DParameter Output Tests", "[audio_parameter][gl_test][output][template]",
                   OutputTestParam1, OutputTestParam2, OutputTestParam3) {
    const char* vert_src = R"(
        #version 300 es
        precision mediump float;
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main()
        {
            gl_Position = vec4(aPos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";
    const char* frag_src = R"(
        #version 300 es
        precision mediump float;
        in vec2 TexCoord;
        out vec4 color;
        void main() {
            color = vec4(sin(TexCoord.x * 2.0 * 3.14159265359), 0, 0, 1);
        }
    )";

    // Get test parameters for this template instantiation
    constexpr auto params = get_output_test_params(TestType::value);
    constexpr int WIDTH = params.width;
    constexpr int HEIGHT = params.height;

    SECTION("RGBA32F OUTPUT") {
        SDLWindow window(WIDTH, HEIGHT);

        GLContext context;
        AudioShaderProgram shader_prog(vert_src, frag_src);
        REQUIRE(shader_prog.initialize());

        GLFramebuffer framebuffer = GLFramebuffer();

        AudioTexture2DParameter output_param(
            "color",
            AudioParameter::ConnectionType::OUTPUT,
            WIDTH, HEIGHT,
            0, // active_texture
            0, // color_attachment
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(output_param.initialize(framebuffer.fbo, &shader_prog));

        framebuffer.bind();

        REQUIRE(output_param.bind());

        shader_prog.use_program();

        context.prepare_draw();
        output_param.render();

        std::vector<GLenum> drawBuffers = {GL_COLOR_ATTACHMENT0 + output_param.get_color_attachment()};
        context.set_draw_buffers(drawBuffers);
        context.draw();

        const float* pixels = static_cast<const float*>(output_param.get_value());
        
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int idx = (y * WIDTH + x) * 4;
                float expectedRed = sin(static_cast<float>(x) / WIDTH * 2.0f * M_PI);
                float actualRed = pixels[idx + 0];
                REQUIRE(actualRed == Catch::Approx(expectedRed).margin(0.05f));
                REQUIRE(pixels[idx + 1] == 0.0f);
                REQUIRE(pixels[idx + 2] == 0.0f);
                REQUIRE(pixels[idx + 3] == 1.0f);
            }
        }

        output_param.unbind();
        framebuffer.unbind();
    }
}

TEST_CASE("AudioTexture2DParameter Input Test", "[audio_parameter][gl_test][input]") {
    const char* vert_src = R"(
        #version 300 es
        precision mediump float;
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main()
        {
            gl_Position = vec4(aPos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";

    SECTION("RGBA32F, 128x1, INPUT") {
        SDLWindow window(128, 1);

        // Vertex shader: pass through position and texcoord
        // Fragment shader: sample input texture and write to output
        const char* frag_src_io = R"(
            #version 300 es
            precision mediump float;
            in vec2 TexCoord;
            uniform sampler2D input_tex;
            out vec4 color;
            void main() {
                color = texture(input_tex, TexCoord);
            }
        )";

        GLContext context;
        AudioShaderProgram shader_prog(vert_src, frag_src_io);
        REQUIRE(shader_prog.initialize());
        GLFramebuffer framebuffer;

        // Prepare input data: fill with a gradient
        std::vector<float> input_data(128 * 1 * 4);
        for (int x = 0; x < 128; ++x) {
            float v = static_cast<float>(x) / 127.0f;
            input_data[x * 4 + 0] = v;
            input_data[x * 4 + 1] = 1.0f - v;
            input_data[x * 4 + 2] = 0.5f;
            input_data[x * 4 + 3] = 1.0f;
        }

        // Input parameter (as texture)
        AudioTexture2DParameter input_param(
            "input_tex",
            AudioParameter::ConnectionType::INPUT,
            128, 1,
            1, // active_texture (use texture unit 1)
            0, // color_attachment (not used for input)
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(input_param.initialize(0, &shader_prog));
        REQUIRE(input_param.set_value(input_data.data()));

        // Output parameter (as texture)
        AudioTexture2DParameter output_param(
            "color",
            AudioParameter::ConnectionType::OUTPUT,
            128, 1,
            0, // active_texture
            0, // color_attachment
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(output_param.initialize(framebuffer.fbo, &shader_prog));

        framebuffer.bind();

        // Bind input and output
        REQUIRE(input_param.bind());
        REQUIRE(output_param.bind());

        shader_prog.use_program();

        context.prepare_draw();
        input_param.render();
        output_param.render();

        std::vector<GLenum> drawBuffers = {GL_COLOR_ATTACHMENT0 + output_param.get_color_attachment()};
        context.set_draw_buffers(drawBuffers);
        context.draw();

        const float* pixels = static_cast<const float*>(output_param.get_value());

        // Check output matches input
        for (int x = 0; x < 128; ++x) {
            int idx = x * 4;
            REQUIRE(pixels[idx + 0] == Catch::Approx(input_data[idx + 0]).margin(0.01f));
            REQUIRE(pixels[idx + 1] == Catch::Approx(input_data[idx + 1]).margin(0.01f));
            REQUIRE(pixels[idx + 2] == Catch::Approx(input_data[idx + 2]).margin(0.01f));
            REQUIRE(pixels[idx + 3] == Catch::Approx(input_data[idx + 3]).margin(0.01f));
        }

        input_param.unbind();
        output_param.unbind();
        framebuffer.unbind();
    }
}

// Multi I/O test parameter structure
struct MultiIOTestParams {
    int width;
    int height;
    const char* name;
};

// Define parameter combinations for Multi I/O tests
using MultiIOTestParam1 = std::integral_constant<int, 0>; // 64x2
using MultiIOTestParam2 = std::integral_constant<int, 1>; // 128x1
using MultiIOTestParam3 = std::integral_constant<int, 2>; // 32x4

// Parameter lookup function for Multi I/O tests
constexpr MultiIOTestParams get_multi_io_test_params(int index) {
    constexpr MultiIOTestParams params[] = {
        {64, 2, "64x2"},
        {128, 1, "128x1"},
        {32, 4, "32x4"}
    };
    return params[index];
}

TEMPLATE_TEST_CASE("Multiple Inputs to Multiple Outputs with OpenGL context", "[audio_parameter][gl_test][multi_io][template]",
                   MultiIOTestParam1, MultiIOTestParam2, MultiIOTestParam3) {
    const char* vert_src = R"(
        #version 300 es
        precision mediump float;
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main()
        {
            gl_Position = vec4(aPos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";

    // Get test parameters for this template instantiation
    constexpr auto params = get_multi_io_test_params(TestType::value);
    constexpr int WIDTH = params.width;
    constexpr int HEIGHT = params.height;

    SECTION("RGBA32F BASIC_MULTIPLE_IO") {
        SDLWindow window(WIDTH, HEIGHT);

        // Fragment shader: sample multiple input textures and write to multiple outputs
        const char* frag_src_multi = R"(
            #version 300 es
            precision mediump float;
            in vec2 TexCoord;
            uniform sampler2D input_a;
            uniform sampler2D input_b;
            uniform sampler2D input_c;
            layout(location = 0) out vec4 output_1;
            layout(location = 1) out vec4 output_2;
            layout(location = 2) out vec4 output_3;
            void main() {
                vec4 a = texture(input_a, TexCoord);
                vec4 b = texture(input_b, TexCoord);
                vec4 c = texture(input_c, TexCoord);
                
                // Output 1: A + B
                output_1 = a + b;
                
                // Output 2: B * C
                output_2 = b * c;
                
                // Output 3: A - C
                output_3 = a - c;
            }
        )";

        GLContext context;
        AudioShaderProgram shader_prog(vert_src, frag_src_multi);
        REQUIRE(shader_prog.initialize());
        GLFramebuffer framebuffer;

        // Prepare input data A: sine wave pattern
        std::vector<float> input_a_data(WIDTH * HEIGHT * 4);
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int idx = (y * WIDTH + x) * 4;
                float v = sin(static_cast<float>(x) / (WIDTH - 1.0f) * 2.0f * M_PI);
                input_a_data[idx + 0] = v;           // Red: sine wave
                input_a_data[idx + 1] = 0.0f;       // Green: 0
                input_a_data[idx + 2] = 0.0f;       // Blue: 0
                input_a_data[idx + 3] = 1.0f;       // Alpha: 1
            }
        }

        // Prepare input data B: cosine wave pattern
        std::vector<float> input_b_data(WIDTH * HEIGHT * 4);
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int idx = (y * WIDTH + x) * 4;
                float v = cos(static_cast<float>(x) / (WIDTH - 1.0f) * 2.0f * M_PI);
                input_b_data[idx + 0] = 0.0f;       // Red: 0
                input_b_data[idx + 1] = v;          // Green: cosine wave
                input_b_data[idx + 2] = 0.0f;       // Blue: 0
                input_b_data[idx + 3] = 1.0f;       // Alpha: 1
            }
        }

        // Prepare input data C: linear gradient pattern
        std::vector<float> input_c_data(WIDTH * HEIGHT * 4);
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int idx = (y * WIDTH + x) * 4;
                float v = static_cast<float>(x) / (WIDTH - 1.0f);
                input_c_data[idx + 0] = 0.0f;       // Red: 0
                input_c_data[idx + 1] = 0.0f;       // Green: 0
                input_c_data[idx + 2] = v;          // Blue: linear gradient
                input_c_data[idx + 3] = 1.0f;       // Alpha: 1
            }
        }

        // Input parameter A
        AudioTexture2DParameter input_a_param(
            "input_a",
            AudioParameter::ConnectionType::INPUT,
            WIDTH, HEIGHT,
            1, // active_texture (use texture unit 1)
            0, // color_attachment (not used for input)
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(input_a_param.initialize(0, &shader_prog));
        REQUIRE(input_a_param.set_value(input_a_data.data()));

        // Input parameter B
        AudioTexture2DParameter input_b_param(
            "input_b",
            AudioParameter::ConnectionType::INPUT,
            WIDTH, HEIGHT,
            2, // active_texture (use texture unit 2)
            0, // color_attachment (not used for input)
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(input_b_param.initialize(0, &shader_prog));
        REQUIRE(input_b_param.set_value(input_b_data.data()));

        // Input parameter C
        AudioTexture2DParameter input_c_param(
            "input_c",
            AudioParameter::ConnectionType::INPUT,
            WIDTH, HEIGHT,
            3, // active_texture (use texture unit 3)
            0, // color_attachment (not used for input)
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(input_c_param.initialize(0, &shader_prog));
        REQUIRE(input_c_param.set_value(input_c_data.data()));

        // Output parameter 1 (A + B)
        AudioTexture2DParameter output_1_param(
            "output_1",
            AudioParameter::ConnectionType::OUTPUT,
            WIDTH, HEIGHT,
            0, // active_texture
            0, // color_attachment
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(output_1_param.initialize(framebuffer.fbo, &shader_prog));

        // Output parameter 2 (B * C)
        AudioTexture2DParameter output_2_param(
            "output_2",
            AudioParameter::ConnectionType::OUTPUT,
            WIDTH, HEIGHT,
            0, // active_texture
            1, // color_attachment
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(output_2_param.initialize(framebuffer.fbo, &shader_prog));

        // Output parameter 3 (A - C)
        AudioTexture2DParameter output_3_param(
            "output_3",
            AudioParameter::ConnectionType::OUTPUT,
            WIDTH, HEIGHT,
            0, // active_texture
            2, // color_attachment
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(output_3_param.initialize(framebuffer.fbo, &shader_prog));

        framebuffer.bind();

        // Bind all parameters
        REQUIRE(input_a_param.bind());
        REQUIRE(input_b_param.bind());
        REQUIRE(input_c_param.bind());
        REQUIRE(output_1_param.bind());
        REQUIRE(output_2_param.bind());
        REQUIRE(output_3_param.bind());

        shader_prog.use_program();

        context.prepare_draw();
        input_a_param.render();
        input_b_param.render();
        input_c_param.render();
        output_1_param.render();
        output_2_param.render();
        output_3_param.render();

        std::vector<GLenum> drawBuffers = {
            GL_COLOR_ATTACHMENT0 + output_1_param.get_color_attachment(),
            GL_COLOR_ATTACHMENT0 + output_2_param.get_color_attachment(),
            GL_COLOR_ATTACHMENT0 + output_3_param.get_color_attachment()
        };
        context.set_draw_buffers(drawBuffers);
        context.draw();

        // Check output 1 (A + B)
        const float* pixels_1 = static_cast<const float*>(output_1_param.get_value());
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int idx = (y * WIDTH + x) * 4;
                int data_idx = (y * WIDTH + x) * 4;
                
                float expected_red = input_a_data[data_idx + 0] + input_b_data[data_idx + 0];
                float expected_green = input_a_data[data_idx + 1] + input_b_data[data_idx + 1];
                float expected_blue = input_a_data[data_idx + 2] + input_b_data[data_idx + 2];
                float expected_alpha = input_a_data[data_idx + 3] + input_b_data[data_idx + 3];
                
                REQUIRE(pixels_1[idx + 0] == Catch::Approx(expected_red).margin(0.01f));
                REQUIRE(pixels_1[idx + 1] == Catch::Approx(expected_green).margin(0.01f));
                REQUIRE(pixels_1[idx + 2] == Catch::Approx(expected_blue).margin(0.01f));
                REQUIRE(pixels_1[idx + 3] == Catch::Approx(expected_alpha).margin(0.01f));
            }
        }

        // Check output 2 (B * C)
        const float* pixels_2 = static_cast<const float*>(output_2_param.get_value());
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int idx = (y * WIDTH + x) * 4;
                int data_idx = (y * WIDTH + x) * 4;
                
                float expected_red = input_b_data[data_idx + 0] * input_c_data[data_idx + 0];
                float expected_green = input_b_data[data_idx + 1] * input_c_data[data_idx + 1];
                float expected_blue = input_b_data[data_idx + 2] * input_c_data[data_idx + 2];
                float expected_alpha = input_b_data[data_idx + 3] * input_c_data[data_idx + 3];
                
                REQUIRE(pixels_2[idx + 0] == Catch::Approx(expected_red).margin(0.01f));
                REQUIRE(pixels_2[idx + 1] == Catch::Approx(expected_green).margin(0.01f));
                REQUIRE(pixels_2[idx + 2] == Catch::Approx(expected_blue).margin(0.01f));
                REQUIRE(pixels_2[idx + 3] == Catch::Approx(expected_alpha).margin(0.01f));
            }
        }

        // Check output 3 (A - C)
        const float* pixels_3 = static_cast<const float*>(output_3_param.get_value());
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int idx = (y * WIDTH + x) * 4;
                int data_idx = (y * WIDTH + x) * 4;
                
                float expected_red = input_a_data[data_idx + 0] - input_c_data[data_idx + 0];
                float expected_green = input_a_data[data_idx + 1] - input_c_data[data_idx + 1];
                float expected_blue = input_a_data[data_idx + 2] - input_c_data[data_idx + 2];
                float expected_alpha = input_a_data[data_idx + 3] - input_c_data[data_idx + 3];
                
                REQUIRE(pixels_3[idx + 0] == Catch::Approx(expected_red).margin(0.01f));
                REQUIRE(pixels_3[idx + 1] == Catch::Approx(expected_green).margin(0.01f));
                REQUIRE(pixels_3[idx + 2] == Catch::Approx(expected_blue).margin(0.01f));
                REQUIRE(pixels_3[idx + 3] == Catch::Approx(expected_alpha).margin(0.01f));
            }
        }

        input_a_param.unbind();
        input_b_param.unbind();
        input_c_param.unbind();
        output_1_param.unbind();
        output_2_param.unbind();
        output_3_param.unbind();
        framebuffer.unbind();
    }

    SECTION("RGBA32F DYNAMIC_INPUT_UPDATE") {
        SDLWindow window(WIDTH, HEIGHT);

        // Fragment shader: sample multiple input textures and write to multiple outputs
        const char* frag_src_multi = R"(
            #version 300 es
            precision mediump float;
            in vec2 TexCoord;
            uniform sampler2D input_a;
            uniform sampler2D input_b;
            uniform sampler2D input_c;
            layout(location = 0) out vec4 output_1;
            layout(location = 1) out vec4 output_2;
            layout(location = 2) out vec4 output_3;
            void main() {
                vec4 a = texture(input_a, TexCoord);
                vec4 b = texture(input_b, TexCoord);
                vec4 c = texture(input_c, TexCoord);
                
                // Output 1: A + B
                output_1 = a + b;
                
                // Output 2: B * C
                output_2 = b * c;
                
                // Output 3: A - C
                output_3 = a - c;
            }
        )";

        GLContext context;
        AudioShaderProgram shader_prog(vert_src, frag_src_multi);
        REQUIRE(shader_prog.initialize());
        GLFramebuffer framebuffer;

        std::vector<GLenum> drawBuffers;
        // Prepare input data A: sine wave pattern
        std::vector<float> input_a_data(WIDTH * HEIGHT * 4);
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int idx = (y * WIDTH + x) * 4;
                float v = sin(static_cast<float>(x) / (WIDTH - 1.0f) * 2.0f * M_PI);
                input_a_data[idx + 0] = v;           // Red: sine wave
                input_a_data[idx + 1] = 0.0f;       // Green: 0
                input_a_data[idx + 2] = 0.0f;       // Blue: 0
                input_a_data[idx + 3] = 1.0f;       // Alpha: 1
            }
        }

        // Prepare input data B: cosine wave pattern
        std::vector<float> input_b_data(WIDTH * HEIGHT * 4);
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int idx = (y * WIDTH + x) * 4;
                float v = cos(static_cast<float>(x) / (WIDTH - 1.0f) * 2.0f * M_PI);
                input_b_data[idx + 0] = 0.0f;       // Red: 0
                input_b_data[idx + 1] = v;          // Green: cosine wave
                input_b_data[idx + 2] = 0.0f;       // Blue: 0
                input_b_data[idx + 3] = 1.0f;       // Alpha: 1
            }
        }

        // Prepare input data C: linear gradient pattern
        std::vector<float> input_c_data(WIDTH * HEIGHT * 4);
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int idx = (y * WIDTH + x) * 4;
                float v = static_cast<float>(x) / (WIDTH - 1.0f);
                input_c_data[idx + 0] = 0.0f;       // Red: 0
                input_c_data[idx + 1] = 0.0f;       // Green: 0
                input_c_data[idx + 2] = v;          // Blue: linear gradient
                input_c_data[idx + 3] = 1.0f;       // Alpha: 1
            }
        }

        // Input parameter A
        AudioTexture2DParameter input_a_param(
            "input_a",
            AudioParameter::ConnectionType::INPUT,
            WIDTH, HEIGHT,
            1, // active_texture (use texture unit 1)
            0, // color_attachment (not used for input)
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(input_a_param.initialize(0, &shader_prog));
        REQUIRE(input_a_param.set_value(input_a_data.data()));

        // Input parameter C
        AudioTexture2DParameter input_c_param(
            "input_c",
            AudioParameter::ConnectionType::INPUT,
            WIDTH, HEIGHT,
            3, // active_texture (use texture unit 3)
            0, // color_attachment (not used for input)
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(input_c_param.initialize(0, &shader_prog));
        REQUIRE(input_c_param.set_value(input_c_data.data()));


        // Input parameter B
        AudioTexture2DParameter input_b_param(
            "input_b",
            AudioParameter::ConnectionType::INPUT,
            WIDTH, HEIGHT,
            2, // active_texture (use texture unit 2)
            0, // color_attachment (not used for input)
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(input_b_param.initialize(0, &shader_prog));
        REQUIRE(input_b_param.set_value(input_b_data.data()));

        // Output parameter 2 (B * C)
        AudioTexture2DParameter output_2_param(
            "output_2",
            AudioParameter::ConnectionType::OUTPUT,
            WIDTH, HEIGHT,
            0, // active_texture
            1, // color_attachment
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(output_2_param.initialize(framebuffer.fbo, &shader_prog));

        // Output parameter 1 (A + B)
        AudioTexture2DParameter output_1_param(
            "output_1",
            AudioParameter::ConnectionType::OUTPUT,
            WIDTH, HEIGHT,
            0, // active_texture
            0, // color_attachment
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(output_1_param.initialize(framebuffer.fbo, &shader_prog));

        // Output parameter 3 (A - C)
        AudioTexture2DParameter output_3_param(
            "output_3",
            AudioParameter::ConnectionType::OUTPUT,
            WIDTH, HEIGHT,
            0, // active_texture
            2, // color_attachment
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(output_3_param.initialize(framebuffer.fbo, &shader_prog));

        framebuffer.bind();

        // Bind all parameters
        REQUIRE(input_a_param.bind());
        REQUIRE(input_b_param.bind());
        REQUIRE(input_c_param.bind());
        REQUIRE(output_2_param.bind());
        REQUIRE(output_1_param.bind());
        REQUIRE(output_3_param.bind());

        shader_prog.use_program();

        context.prepare_draw();
        input_a_param.render();
        input_b_param.render();
        input_c_param.render();
        output_1_param.render();
        output_3_param.render();
        output_2_param.render();

        drawBuffers = {
            GL_COLOR_ATTACHMENT0 + output_1_param.get_color_attachment(),
            GL_COLOR_ATTACHMENT0 + output_2_param.get_color_attachment(),
            GL_COLOR_ATTACHMENT0 + output_3_param.get_color_attachment()
        };
        context.set_draw_buffers(drawBuffers);
        context.draw();

        // Test dynamic updates: change input B and verify outputs update correctly
        std::vector<float> new_input_b_data(WIDTH * HEIGHT * 4);
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int idx = (y * WIDTH + x) * 4;
                float v = static_cast<float>(y) / 1.0f; // Constant value based on y
                new_input_b_data[idx + 0] = v;          // Red: y-based constant
                new_input_b_data[idx + 1] = 0.5f;      // Green: constant 0.5
                new_input_b_data[idx + 2] = 0.0f;      // Blue: 0
                new_input_b_data[idx + 3] = 1.0f;      // Alpha: 1
            }
        }

        REQUIRE(input_b_param.set_value(new_input_b_data.data()));

        framebuffer.bind();
        
        // Bind all parameters
        REQUIRE(input_a_param.bind());
        REQUIRE(input_b_param.bind());
        REQUIRE(input_c_param.bind());
        REQUIRE(output_1_param.bind());
        REQUIRE(output_2_param.bind());
        REQUIRE(output_3_param.bind());

        shader_prog.use_program();

        context.prepare_draw();
        input_a_param.render();
        input_b_param.render();
        input_c_param.render();
        // Mix the order of the rendering up
        output_1_param.render();
        output_3_param.render();
        output_2_param.render();

        drawBuffers = {
            GL_COLOR_ATTACHMENT0 + output_1_param.get_color_attachment(),
            GL_COLOR_ATTACHMENT0 + output_2_param.get_color_attachment(),
            GL_COLOR_ATTACHMENT0 + output_3_param.get_color_attachment()
        };
        context.set_draw_buffers(drawBuffers);
        context.draw();

        // Verify output 1 (A + new_B) updated correctly
        const float* new_pixels_1 = static_cast<const float*>(output_1_param.get_value());
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int idx = (y * WIDTH + x) * 4;
                int data_idx = (y * WIDTH + x) * 4;
                
                float expected_red = input_a_data[data_idx + 0] + new_input_b_data[data_idx + 0];
                float expected_green = input_a_data[data_idx + 1] + new_input_b_data[data_idx + 1];
                float expected_blue = input_a_data[data_idx + 2] + new_input_b_data[data_idx + 2];
                float expected_alpha = input_a_data[data_idx + 3] + new_input_b_data[data_idx + 3];
                
                REQUIRE(new_pixels_1[idx + 0] == Catch::Approx(expected_red).margin(0.01f));
                REQUIRE(new_pixels_1[idx + 1] == Catch::Approx(expected_green).margin(0.01f));
                REQUIRE(new_pixels_1[idx + 2] == Catch::Approx(expected_blue).margin(0.01f));
                REQUIRE(new_pixels_1[idx + 3] == Catch::Approx(expected_alpha).margin(0.01f));
            }
        }

        // Verify output 2 (new_B * C) updated correctly
        const float* new_pixels_2 = static_cast<const float*>(output_2_param.get_value());
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int idx = (y * WIDTH + x) * 4;
                int data_idx = (y * WIDTH + x) * 4;

                float expected_red = new_input_b_data[data_idx + 0] * input_c_data[data_idx + 0];
                float expected_green = new_input_b_data[data_idx + 1] * input_c_data[data_idx + 1];
                float expected_blue = new_input_b_data[data_idx + 2] * input_c_data[data_idx + 2];
                float expected_alpha = new_input_b_data[data_idx + 3] * input_c_data[data_idx + 3];

                REQUIRE(new_pixels_2[idx + 0] == Catch::Approx(expected_red).margin(0.01f));
                REQUIRE(new_pixels_2[idx + 1] == Catch::Approx(expected_green).margin(0.01f));
                REQUIRE(new_pixels_2[idx + 2] == Catch::Approx(expected_blue).margin(0.01f));
                REQUIRE(new_pixels_2[idx + 3] == Catch::Approx(expected_alpha).margin(0.01f));
            }
        }

        // Verify output 3 (A - C) remains unchanged
        const float* new_pixels_3 = static_cast<const float*>(output_3_param.get_value());
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int idx = (y * WIDTH + x) * 4;
                int data_idx = (y * WIDTH + x) * 4;

                float expected_red = input_a_data[data_idx + 0] - input_c_data[data_idx + 0];
                float expected_green = input_a_data[data_idx + 1] - input_c_data[data_idx + 1];
                float expected_blue = input_a_data[data_idx + 2] - input_c_data[data_idx + 2];
                float expected_alpha = input_a_data[data_idx + 3] - input_c_data[data_idx + 3];

                REQUIRE(new_pixels_3[idx + 0] == Catch::Approx(expected_red).margin(0.01f));
                REQUIRE(new_pixels_3[idx + 1] == Catch::Approx(expected_green).margin(0.01f));
                REQUIRE(new_pixels_3[idx + 2] == Catch::Approx(expected_blue).margin(0.01f));
                REQUIRE(new_pixels_3[idx + 3] == Catch::Approx(expected_alpha).margin(0.01f));
            }
        }

        input_a_param.unbind();
        input_b_param.unbind();
        input_c_param.unbind();
        output_1_param.unbind();
        output_2_param.unbind();
        output_3_param.unbind();
        framebuffer.unbind();
    }
}

// Pipeline test parameter structure
struct PipelineTestParams {
    int width;
    int height;
    const char* name;
};

// Define parameter combinations for Pipeline tests
using PipelineTestParam1 = std::integral_constant<int, 0>; // 64x1
using PipelineTestParam2 = std::integral_constant<int, 1>; // 128x1
using PipelineTestParam3 = std::integral_constant<int, 2>; // 32x2

// Parameter lookup function for Pipeline tests
constexpr PipelineTestParams get_pipeline_test_params(int index) {
    constexpr PipelineTestParams params[] = {
        {64, 1, "64x1"},
        {128, 1, "128x1"},
        {32, 2, "32x2"}
    };
    return params[index];
}

TEMPLATE_TEST_CASE("Two-stage pipeline with passthrough linking", "[audio_parameter][gl_test][passthrough_link][template]",
                   PipelineTestParam1, PipelineTestParam2, PipelineTestParam3) {
    const char* vert_src = R"(
        #version 300 es
        precision mediump float;
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";

    // Get test parameters for this template instantiation
    constexpr auto params = get_pipeline_test_params(TestType::value);
    constexpr int WIDTH = params.width;
    constexpr int HEIGHT = params.height;

    SECTION("RGBA32F SCALE_HALF") {

        SDLWindow window(WIDTH, HEIGHT);

        // Stage 1: generates a sine wave pattern in the red channel
        const char* frag_stage1 = R"(
            #version 300 es
            precision mediump float;
            in vec2 TexCoord;
            out vec4 color;
            void main() {
                color = vec4(sin(TexCoord.x * 2.0 * 3.14159265359), 0.0, 0.0, 1.0);
            }
        )";

        // Stage 2: samples the shared texture and scales the red channel by 0.5
        const char* frag_stage2 = R"(
            #version 300 es
            precision mediump float;
            in vec2 TexCoord;
            uniform sampler2D shared_tex;
            out vec4 color;
            void main() {
                float r = texture(shared_tex, TexCoord).r;
                color = vec4(r * 0.5, 0.0, 0.0, 1.0);
            }
        )";

        // Create single GL context and framebuffers for the two stages
        GLContext context;
        AudioShaderProgram shader_prog1(vert_src, frag_stage1);
        REQUIRE(shader_prog1.initialize());
        GLFramebuffer framebuffer1;

        AudioShaderProgram shader_prog2(vert_src, frag_stage2);
        REQUIRE(shader_prog2.initialize());
        GLFramebuffer framebuffer2;

        // Passthrough parameter that will receive Stage 1 output and be read in Stage 2
        AudioTexture2DParameter passthrough_param(
            "shared_tex",
            AudioParameter::ConnectionType::PASSTHROUGH,
            WIDTH, HEIGHT,
            0,  // active texture unit for Stage 2 sampling
            0,  // color attachment (not in draw buffers)
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(passthrough_param.initialize(framebuffer2.fbo, &shader_prog2));

        // Stage 1 output texture that will be linked to the passthrough parameter
        AudioTexture2DParameter stage1_output(
            "color",
            AudioParameter::ConnectionType::OUTPUT,
            WIDTH, HEIGHT,
            0,  // active texture (unused for OUTPUT)
            0,  // color attachment
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(stage1_output.initialize(framebuffer1.fbo, &shader_prog1));

        // Link Stage 1 output to the passthrough parameter texture
        REQUIRE(stage1_output.link(&passthrough_param));

        // Stage 2 final output texture
        AudioTexture2DParameter stage2_output(
            "color",
            AudioParameter::ConnectionType::OUTPUT,
            WIDTH, HEIGHT,
            0,  // active texture (unused for OUTPUT)
            0,  // color attachment distinct from passthrough texture
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(stage2_output.initialize(framebuffer2.fbo, &shader_prog2));

        // ---------------- Stage 1 render ----------------
        framebuffer1.bind();
        REQUIRE(stage1_output.bind());
        shader_prog1.use_program();
        context.prepare_draw();
        stage1_output.render();
        std::vector<GLenum> drawBuffers1 = { GL_COLOR_ATTACHMENT0 + stage1_output.get_color_attachment() };
        context.set_draw_buffers(drawBuffers1);
        context.draw();

        // Optional: verify Stage 1 output texture values
        const float* stage1_pixels = static_cast<const float*>(stage1_output.get_value());
        for (int x = 0; x < WIDTH; ++x) {
            // TexCoord.x for pixel x is (x + 0.5) / WIDTH
            float tex_coord_x = (static_cast<float>(x) + 0.5f) / WIDTH;
            float expected = sin(tex_coord_x * 2.0f * static_cast<float>(M_PI));
            REQUIRE(stage1_pixels[x * 4 + 0] == Catch::Approx(expected).margin(0.05f));
        }

        // Verify passthrough parameter values after Stage 1 render
        const float* passthrough_pixels = static_cast<const float*>(passthrough_param.get_value());
        for (int x = 0; x < WIDTH; ++x) {
            // TexCoord.x for pixel x is (x + 0.5) / WIDTH
            float tex_coord_x = (static_cast<float>(x) + 0.5f) / WIDTH;
            float expected = sin(tex_coord_x * 2.0f * static_cast<float>(M_PI));
            REQUIRE(passthrough_pixels[x * 4 + 0] == Catch::Approx(expected).margin(0.05f));
        }

        // ---------------- Stage 2 render ----------------
        framebuffer2.bind();
        REQUIRE(passthrough_param.bind());
        REQUIRE(stage2_output.bind());

        shader_prog2.use_program();
        context.prepare_draw();
        // Render parameters: upload uniforms/textures
        passthrough_param.render();
        stage2_output.render();

        std::vector<GLenum> drawBuffers2 = { GL_COLOR_ATTACHMENT0 + stage2_output.get_color_attachment() };
        context.set_draw_buffers(drawBuffers2);
        context.draw();

        // Validate Stage 2 output values (should be half of Stage 1)
        const float* stage2_pixels = static_cast<const float*>(stage2_output.get_value());
        for (int x = 0; x < WIDTH; ++x) {
            // TexCoord.x for pixel x is (x + 0.5) / WIDTH
            float tex_coord_x = (static_cast<float>(x) + 0.5f) / WIDTH;
            float expected_red = sin(tex_coord_x * 2.0f * static_cast<float>(M_PI)) * 0.5f;
            REQUIRE(stage2_pixels[x * 4 + 0] == Catch::Approx(expected_red).margin(0.05f));
            REQUIRE(stage2_pixels[x * 4 + 1] == 0.0f);
            REQUIRE(stage2_pixels[x * 4 + 2] == 0.0f);
            REQUIRE(stage2_pixels[x * 4 + 3] == 1.0f);
        }

        passthrough_param.unbind();
        stage2_output.unbind();
        framebuffer2.unbind();
    }

    SECTION("RGBA32F, 128x2, NEGATE") {
        constexpr int WIDTH = 128;
        constexpr int HEIGHT = 2;

        SDLWindow window(WIDTH, HEIGHT);

        // Stage 1: cosine pattern
        const char* frag_stage1 = R"(
            #version 300 es
            precision mediump float;
            in vec2 TexCoord;
            out vec4 color;
            void main() {
                color = vec4(cos(TexCoord.x * 2.0 * 3.14159265359), 0.0, 0.0, 1.0);
            }
        )";

        // Stage 2: negate the sampled red channel
        const char* frag_stage2 = R"(
            #version 300 es
            precision mediump float;
            in vec2 TexCoord;
            uniform sampler2D shared_tex;
            out vec4 color;
            void main() {
                float r = texture(shared_tex, TexCoord).r;
                color = vec4(-r, 0.0, 0.0, 1.0);
            }
        )";

        GLContext context;
        AudioShaderProgram shader_prog1(vert_src, frag_stage1);
        REQUIRE(shader_prog1.initialize());
        GLFramebuffer framebuffer1;

        AudioShaderProgram shader_prog2(vert_src, frag_stage2);
        REQUIRE(shader_prog2.initialize());
        GLFramebuffer framebuffer2;

        AudioTexture2DParameter passthrough_param(
            "shared_tex",
            AudioParameter::ConnectionType::PASSTHROUGH,
            WIDTH, HEIGHT,
            1,  // active texture unit
            1,  // color attachment (not in draw buffers)
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(passthrough_param.initialize(framebuffer2.fbo, &shader_prog2));

        AudioTexture2DParameter stage1_output(
            "color",
            AudioParameter::ConnectionType::OUTPUT,
            WIDTH, HEIGHT,
            0,
            0,
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(stage1_output.initialize(framebuffer1.fbo, &shader_prog1));
        REQUIRE(stage1_output.link(&passthrough_param));

        // Stage 2 output
        AudioTexture2DParameter stage2_output(
            "color",
            AudioParameter::ConnectionType::OUTPUT,
            WIDTH, HEIGHT,
            0,
            0,
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(stage2_output.initialize(framebuffer2.fbo, &shader_prog2));


        // Render Stage 1
        framebuffer1.bind();
        REQUIRE(stage1_output.bind());
        shader_prog1.use_program();
        context.prepare_draw();
        stage1_output.render();
        std::vector<GLenum> drawBuffers1 = { GL_COLOR_ATTACHMENT0 + stage1_output.get_color_attachment() };
        context.set_draw_buffers(drawBuffers1);
        context.draw();

        // Render Stage 2
        framebuffer2.bind();
        REQUIRE(passthrough_param.bind());
        REQUIRE(stage2_output.bind());
        shader_prog2.use_program();
        context.prepare_draw();
        passthrough_param.render();
        stage2_output.render();
        std::vector<GLenum> drawBuffers2 = { GL_COLOR_ATTACHMENT0 + stage2_output.get_color_attachment() };
        context.set_draw_buffers(drawBuffers2);
        context.draw();

        // Validate Stage 2 output (negated cosine)
        const float* stage2_pixels = static_cast<const float*>(stage2_output.get_value());
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int idx = (y * WIDTH + x) * 4;
                float expected_red = -cos(static_cast<float>(x) / WIDTH * 2.0f * static_cast<float>(M_PI));
                REQUIRE(stage2_pixels[idx + 0] == Catch::Approx(expected_red).margin(0.05f));
                REQUIRE(stage2_pixels[idx + 1] == 0.0f);
                REQUIRE(stage2_pixels[idx + 2] == 0.0f);
                REQUIRE(stage2_pixels[idx + 3] == 1.0f);
            }
        }

        passthrough_param.unbind();
        stage2_output.unbind();
        framebuffer2.unbind();
    }
}

TEST_CASE("AudioIntBufferParameter uniform value across shader stages", "[audio_parameter][gl_test]") {
    const char* vert_src = R"(
        #version 300 es
        precision mediump float;
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";
    // Stage 1: Write int buffer value to red channel
    const char* frag_stage1 = R"(
        #version 300 es
        precision mediump float;
        layout(std140) uniform global_time {
            int global_time_val;
        };
        in vec2 TexCoord;
        layout(location = 0) out vec4 color;
        void main() {
            color = vec4(float(global_time_val), 0, 0, 1);
        }
    )";
    // Stage 2: Read from previous output and write to green channel
    const char* frag_stage2 = R"(
        #version 300 es
        precision mediump float;
        layout(std140) uniform global_time {
            int global_time_val;
        };
        uniform sampler2D prev_tex;
        in vec2 TexCoord;
        layout(location = 0) out vec4 color;

        void main() {
            float prev = texture(prev_tex, TexCoord).r;
            color = vec4(prev + float(global_time_val), 0, 0, 1);
        }
    )";
    SDLWindow window(8, 1);
    GLContext context;

    AudioShaderProgram shader_prog1(vert_src, frag_stage1);
    AudioShaderProgram shader_prog2(vert_src, frag_stage2);

    REQUIRE(shader_prog1.initialize());
    REQUIRE(shader_prog2.initialize());

    GLFramebuffer framebuffer1, framebuffer2;
    // Stage 1 output
    AudioTexture2DParameter stage1_output(
        "color",
        AudioParameter::ConnectionType::OUTPUT,
        8, 1,
        0, 0,
        GL_NEAREST,
        GL_FLOAT,
        GL_RGBA,
        GL_RGBA32F
    );
    REQUIRE(stage1_output.initialize(framebuffer1.fbo, &shader_prog1));

    // Stage 2 input (linked to stage 1 output)
    AudioTexture2DParameter stage2_input(
        "prev_tex",
        AudioParameter::ConnectionType::PASSTHROUGH,
        8, 1,
        0, 0,
        GL_NEAREST,
        GL_FLOAT,
        GL_RGBA,
        GL_RGBA32F
    );
    REQUIRE(stage2_input.initialize(framebuffer2.fbo, &shader_prog2));

    REQUIRE(stage1_output.link(&stage2_input));
    // Stage 2 output
    AudioTexture2DParameter stage2_output(
        "color",
        AudioParameter::ConnectionType::OUTPUT,
        8, 1,
        0, 0,
        GL_NEAREST,
        GL_FLOAT,
        GL_RGBA,
        GL_RGBA32F
    );
    REQUIRE(stage2_output.initialize(framebuffer2.fbo, &shader_prog2));

    // Add and set int buffer parameter ONCE for both stages
    auto global_time_param = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);

    global_time_param->set_value(0);

    global_time_param->initialize();

    // Stage 1 render
    global_time_param->set_value(42);
    global_time_param->render();

    framebuffer1.bind();
    REQUIRE(stage1_output.bind());
    shader_prog1.use_program();
    context.prepare_draw();
    stage1_output.render();
    std::vector<GLenum> drawBuffers1 = {GL_COLOR_ATTACHMENT0 + stage1_output.get_color_attachment()};
    context.set_draw_buffers(drawBuffers1);
    context.draw();

    // Stage 2 render (reuse same param, do NOT set again)
    global_time_param->set_value(12);
    global_time_param->render();

    framebuffer2.bind();
    REQUIRE(stage2_input.bind());
    REQUIRE(stage2_output.bind());
    shader_prog2.use_program();
    context.prepare_draw();
    stage2_input.render();
    stage2_output.render();
    std::vector<GLenum> drawBuffers2 = {GL_COLOR_ATTACHMENT0 + stage2_output.get_color_attachment()};
    context.set_draw_buffers(drawBuffers2);
    context.draw();

    // Validate: stage 1 output red == 42, stage 2 output green == 42
    const float* stage1_pixels = static_cast<const float*>(stage1_output.get_value());
    const float* stage2_pixels = static_cast<const float*>(stage2_output.get_value());
    for (int i = 0; i < 8; i++) {
        REQUIRE(stage1_pixels[i*4] == Catch::Approx(42.0f).margin(0.01f));
        REQUIRE(stage2_pixels[i*4] == Catch::Approx(42.0f + 12.0f).margin(0.01f));
    }
}

// Copy linking test parameter structure
struct CopyTestParams {
    int width;
    int height;
    const char* name;
};

// Define parameter combinations for Copy tests
using CopyTestParam1 = std::integral_constant<int, 0>; // 256x2
using CopyTestParam2 = std::integral_constant<int, 1>; // 128x1
using CopyTestParam3 = std::integral_constant<int, 2>; // 64x4

// Parameter lookup function for Copy tests
constexpr CopyTestParams get_copy_test_params(int index) {
    constexpr CopyTestParams params[] = {
        {256, 2, "256x2"},
        {128, 1, "128x1"},
        {64, 4, "64x4"}
    };
    return params[index];
}

TEMPLATE_TEST_CASE("Texture2DParameter pass-through copy linking", "[audio_parameter][gl_test][passthrough_copy][template]",
                   CopyTestParam1, CopyTestParam2, CopyTestParam3) {
    // Vertex shader identical to default render-stage vertex
    const char* vert_src = R"(
        #version 300 es
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";

    // Get test parameters for this template instantiation
    constexpr auto params = get_copy_test_params(TestType::value);
    constexpr int WIDTH = params.width;
    constexpr int HEIGHT = params.height;

    SECTION("RGBA32F COPY") {

        SDLWindow window(WIDTH, HEIGHT);

        //----------------------------------------------------
        // Fragment shaders mirroring render-stage test setup
        //----------------------------------------------------

        // Settings imported from shaders/settings (inlined here)
        const char* settings_src = R"(
            #version 300 es
            precision highp float;
            const float PI = 3.14159265359;
            const float TWO_PI = 6.28318530718;
            in vec2 TexCoord;

            int buffer_size = 256;
            int sample_rate = 44100;
            int num_channels = 2;

            uniform sampler2D stream_audio_texture;
            layout(std140) uniform global_time {
                int global_time_val;
            };
            layout(location = 0) out vec4 output_audio_texture;
            layout(location = 1) out vec4 debug_audio_texture;
        )";

        // Stage 1 shader – simple spatial sine wave (matches earlier texture tests)
        std::string frag_stage1 = std::string(settings_src) + R"(
        void main(){
            float sine_wave = sin(TWO_PI * TexCoord.x);
            vec4 stream_audio = texture(stream_audio_texture, TexCoord);

            output_audio_texture = vec4(sine_wave) + stream_audio;
            debug_audio_texture  = vec4(sine_wave) + stream_audio;
        }
        )";

        // Stage 2 shader – pure pass-through copy
        std::string frag_stage2 = std::string(settings_src) + R"(
        void main(){
            vec4 v = texture(stream_audio_texture, TexCoord);
            output_audio_texture = v;
            debug_audio_texture  = vec4(0.0);
        }
        )";

        //----------------------------------------------------
        // Stage 1 setup
        //----------------------------------------------------
        GLContext context;
        AudioShaderProgram shader_prog1(vert_src, frag_stage1.c_str());
        REQUIRE(shader_prog1.initialize());
        GLFramebuffer framebuffer1;

        // stream_audio_texture (unconnected in stage 1, defaults to 0 texture)
        AudioTexture2DParameter stage1_stream_param(
            "stream_audio_texture",
            AudioParameter::ConnectionType::PASSTHROUGH,
            WIDTH, HEIGHT,
            0,   // active texture unit 0
            0,   // colour attachment not drawn
            GL_NEAREST,
            GL_FLOAT,
            GL_RED,
            GL_R32F
        );

        // Outputs for Stage 1
        AudioTexture2DParameter stage1_output(
            "output_audio_texture",
            AudioParameter::ConnectionType::OUTPUT,
            WIDTH, HEIGHT,
            0,
            0,
            GL_NEAREST,
            GL_FLOAT,
            GL_RED,
            GL_R32F
        );

        AudioTexture2DParameter stage1_debug(
            "debug_audio_texture",
            AudioParameter::ConnectionType::OUTPUT,
            WIDTH, HEIGHT,
            0,
            1,
            GL_NEAREST,
            GL_FLOAT,
            GL_RED,
            GL_R32F
        );

        //----------------------------------------------------
        // Stage 2 setup
        //----------------------------------------------------
        AudioShaderProgram shader_prog2(vert_src, frag_stage2.c_str());
        REQUIRE(shader_prog2.initialize());
        GLFramebuffer framebuffer2;

        // Passthrough parameter (receives Stage 1 output)
        AudioTexture2DParameter stage2_stream_param(
            "stream_audio_texture",
            AudioParameter::ConnectionType::PASSTHROUGH,
            WIDTH, HEIGHT,
            0,  // active texture unit 0
            0,  // colour attachment not drawn
            GL_NEAREST,
            GL_FLOAT,
            GL_RED,
            GL_R32F
        );

        // Outputs for Stage 2
        AudioTexture2DParameter stage2_output(
            "output_audio_texture",
            AudioParameter::ConnectionType::OUTPUT,
            WIDTH, HEIGHT,
            0,
            0,
            GL_NEAREST,
            GL_FLOAT,
            GL_RED,
            GL_R32F
        );

        AudioTexture2DParameter stage2_debug(
            "debug_audio_texture",
            AudioParameter::ConnectionType::OUTPUT,
            WIDTH, HEIGHT,
            0,
            1,
            GL_NEAREST,
            GL_FLOAT,
            GL_RED,
            GL_R32F
        );

        REQUIRE(stage1_stream_param.initialize(framebuffer1.fbo, &shader_prog1));
        REQUIRE(stage1_output.initialize(framebuffer1.fbo, &shader_prog1));
        REQUIRE(stage1_debug.initialize(framebuffer1.fbo, &shader_prog1));
        REQUIRE(stage2_stream_param.initialize(framebuffer2.fbo, &shader_prog2));
        REQUIRE(stage2_output.initialize(framebuffer2.fbo, &shader_prog2));
        REQUIRE(stage2_debug.initialize(framebuffer2.fbo, &shader_prog2));

        // Link Stage 1 output to Stage 2 stream texture
        REQUIRE(stage1_output.link(&stage2_stream_param));

        framebuffer1.bind();
        REQUIRE(stage1_stream_param.bind());
        REQUIRE(stage1_output.bind());
        REQUIRE(stage1_debug.bind());

        //----------------------------------------------------
        // Render Stage 1
        //----------------------------------------------------

        shader_prog1.use_program();
        context.prepare_draw();

        // Upload uniforms & textures
        stage1_stream_param.render();
        stage1_output.render();
        stage1_debug.render();

        std::vector<GLenum> draw_buffers1 = {
            GL_COLOR_ATTACHMENT0 + stage1_output.get_color_attachment(),
            GL_COLOR_ATTACHMENT0 + stage1_debug.get_color_attachment()
        };
        context.set_draw_buffers(draw_buffers1);
        context.draw();

        //----------------------------------------------------
        // Validation – Stage 1 output should match sine wave
        //----------------------------------------------------
        const float* stage1_pixels = static_cast<const float*>(stage1_debug.get_value());
        REQUIRE(stage1_pixels != nullptr);
        for (int x = 0; x < WIDTH; ++x) {
            float expected_red = sin(static_cast<float>(x) / WIDTH * 2.0f * static_cast<float>(M_PI));
            REQUIRE(stage1_pixels[x] == Catch::Approx(expected_red).margin(0.1f));
        }

        //----------------------------------------------------
        // Render Stage 2
        //----------------------------------------------------
        framebuffer2.bind();
        REQUIRE(stage2_stream_param.bind());
        REQUIRE(stage2_output.bind());
        REQUIRE(stage2_debug.bind());

        shader_prog2.use_program();
        context.prepare_draw();

        stage2_stream_param.render();
        stage2_output.render();
        stage2_debug.render();

        std::vector<GLenum> draw_buffers2 = {
            GL_COLOR_ATTACHMENT0 + stage2_output.get_color_attachment(),
            GL_COLOR_ATTACHMENT0 + stage2_debug.get_color_attachment()
        };
        context.set_draw_buffers(draw_buffers2);
        context.draw();

        //----------------------------------------------------
        // Validation – Stage 2 output should match Stage 1 sine wave
        //----------------------------------------------------
        const float* stage2_pixels = static_cast<const float*>(stage2_output.get_value());
        REQUIRE(stage2_pixels != nullptr);
        for (int x = 0; x < WIDTH; ++x) {
            float expected_red = sin(static_cast<float>(x) / WIDTH * 2.0f * static_cast<float>(M_PI));
            REQUIRE(stage2_pixels[x] == Catch::Approx(expected_red).margin(0.1f));
        }

        // Clean-up
        stage2_stream_param.unbind();
        stage2_output.unbind();
        stage2_debug.unbind();
        framebuffer2.unbind();
    }
}

TEST_CASE("Unconnected PASSTHROUGH get_value returns 0 values", "[audio_parameter][gl_test][passthrough]") {
    const char* vert_src = R"(
        #version 300 es
        precision mediump float;
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";

    const char* frag_src = R"(
        #version 300 es
        precision mediump float;
        in vec2 TexCoord;
        uniform sampler2D shared_tex;
        out vec4 color;
        void main() {
            // Sample to ensure the uniform is active (non-optimized-out)
            float r = texture(shared_tex, TexCoord).r;
            color = vec4(r, 0.0, 0.0, 1.0);
        }
    )";

    SDLWindow window(8, 1);
    GLContext context;
    AudioShaderProgram shader_prog(vert_src, frag_src);
    REQUIRE(shader_prog.initialize());
    GLFramebuffer framebuffer;

    AudioTexture2DParameter passthrough_param(
        "shared_tex",
        AudioParameter::ConnectionType::PASSTHROUGH,
        8, 1,
        0,  // active texture
        0,  // color attachment
        GL_NEAREST,
        GL_FLOAT,
        GL_RGBA,
        GL_RGBA32F
    );

    REQUIRE(passthrough_param.initialize(framebuffer.fbo, &shader_prog));

    const float* pixels = static_cast<const float*>(passthrough_param.get_value());
    // Check if its a vector of 0s
    for (int i = 0; i < 8; i++) {
        REQUIRE(pixels[i] == 0.0f);
    }

}