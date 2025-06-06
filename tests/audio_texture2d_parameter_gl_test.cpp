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
        layout(location = 0) in vec2 pos;
        out float tex_x;
        void main() {
            gl_Position = vec4(pos, 0, 1);
            tex_x = pos.x * 0.5 + 0.5;
        }
    )";
    const char* frag_src = R"(
        #version 300 es
        precision mediump float;
        in float tex_x;
        out vec4 color;
        void main() {
            color = vec4(sin(tex_x * 2.0 * 3.14159265359), 0, 0, 1);
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
}