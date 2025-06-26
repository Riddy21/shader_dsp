#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"

#include "audio_core/audio_parameter.h"
#include "audio_parameter/audio_texture2d_parameter.h"

#include <functional>
#include <cmath>
/**
 * @brief Tests for texture parameter initialization with OpenGL context
 * 
 * These tests check the texture creation and initialization in an OpenGL context.
 * Note: These tests require a valid OpenGL context to run, which may not be available
 * in all test environments. They are marked with [gl] tag.
 */
TEST_CASE("AudioTexture2DParameter with OpenGL context", "[audio_parameter][gl_test]") {
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

    SECTION("RGBA32F, 256x1, OUTPUT") {
        SDLWindow window(256, 1);

        GLContext context(vert_src, frag_src);

        GLFramebuffer framebuffer = GLFramebuffer();

        AudioTexture2DParameter output_param(
            "color",
            AudioParameter::ConnectionType::OUTPUT,
            256, 1,
            0, // active_texture
            0, // color_attachment
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(output_param.initialize(framebuffer.fbo, &context.shader_prog));

        framebuffer.bind();

        REQUIRE(output_param.bind());

        context.pre_draw();

        output_param.render();

        GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0 + output_param.get_color_attachment()};

        context.draw(drawBuffers);

        const float* pixels = static_cast<const float*>(output_param.get_value());
        
        for (int i = 0; i < 256; ++i) {
            float expectedRed = sin(static_cast<float>(i) / 256 * 2.0f * M_PI);
            float actualRed = pixels[i * 4 + 0];
            REQUIRE(actualRed == Catch::Approx(expectedRed).margin(0.05f));
            REQUIRE(pixels[i * 4 + 1] == 0.0f);
            REQUIRE(pixels[i * 4 + 2] == 0.0f);
            REQUIRE(pixels[i * 4 + 3] == 1.0f);
        }

        output_param.unbind();
    }

    SECTION("RGBA32F, 64x4, OUTPUT") {
        SDLWindow window(64, 4);

        GLContext context(vert_src, frag_src);

        GLFramebuffer framebuffer = GLFramebuffer();

        AudioTexture2DParameter output_param(
            "color",
            AudioParameter::ConnectionType::OUTPUT,
            64, 4,
            0, // active_texture
            0, // color_attachment
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(output_param.initialize(framebuffer.fbo, &context.shader_prog));

        framebuffer.bind();

        REQUIRE(output_param.bind());

        context.pre_draw();

        output_param.render();

        GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0 + output_param.get_color_attachment()};

        context.draw(drawBuffers);

        const float* pixels = static_cast<const float*>(output_param.get_value());
        
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 64; ++x) {
                int idx = (y * 64 + x) * 4;
                float expectedRed = sin(static_cast<float>(x) / 64 * 2.0f * M_PI);
                float actualRed = pixels[idx + 0];
                REQUIRE(actualRed == Catch::Approx(expectedRed).margin(0.05f));
                REQUIRE(pixels[idx + 1] == 0.0f);
                REQUIRE(pixels[idx + 2] == 0.0f);
                REQUIRE(pixels[idx + 3] == 1.0f);
            }
        }

        output_param.unbind();
    }

    // Add more sections/configurations as needed
    SECTION("RGBA32F, 128x2, OUTPUT") {
        SDLWindow window(128, 2);

        GLContext context(vert_src, frag_src);

        GLFramebuffer framebuffer = GLFramebuffer();

        AudioTexture2DParameter output_param(
            "color",
            AudioParameter::ConnectionType::OUTPUT,
            128, 2,
            0, // active_texture
            0, // color_attachment
            GL_NEAREST,
            GL_FLOAT,
            GL_RGBA,
            GL_RGBA32F
        );
        REQUIRE(output_param.initialize(framebuffer.fbo, &context.shader_prog));

        framebuffer.bind();

        REQUIRE(output_param.bind());

        context.pre_draw();

        output_param.render();

        GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0 + output_param.get_color_attachment()};

        context.draw(drawBuffers);

        const float* pixels = static_cast<const float*>(output_param.get_value());
        
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 128; ++x) {
                int idx = (y * 128 + x) * 4;
                float expectedRed = sin(static_cast<float>(x) / 128 * 2.0f * M_PI);
                float actualRed = pixels[idx + 0];
                REQUIRE(actualRed == Catch::Approx(expectedRed).margin(0.05f));
                REQUIRE(pixels[idx + 1] == 0.0f);
                REQUIRE(pixels[idx + 2] == 0.0f);
                REQUIRE(pixels[idx + 3] == 1.0f);
            }
        }

        output_param.unbind();
    }

    SECTION("RGBA32F, 512x3, INPUT") {
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

        GLContext context(vert_src, frag_src_io);
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
        REQUIRE(input_param.initialize(0, &context.shader_prog));
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
        REQUIRE(output_param.initialize(framebuffer.fbo, &context.shader_prog));

        framebuffer.bind();

        // Bind input and output
        REQUIRE(input_param.bind());
        REQUIRE(output_param.bind());

        context.pre_draw();

        output_param.render();

        GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0 + output_param.get_color_attachment()};
        context.draw(drawBuffers);

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
    }
}