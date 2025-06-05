#include "catch2/catch_all.hpp"
#include "audio_core/audio_parameter.h"
#include "audio_parameter/audio_texture2d_parameter.h"
#include "utilities/shader_program.h"

#include "SDL2/SDL.h"
#include "GL/glew.h"

#include <functional>
#include <cmath>

// Helper fixture for OpenGL/texture test setup
class GLTexture2DTestFixture {
public:
    SDL_Window* window = nullptr;
    SDL_GLContext glctx = nullptr;
    GLuint fbo = 0;
    GLuint vao = 0, vbo = 0;
    int width, height;
    AudioShaderProgram shader_prog;
    GLuint prog;

    GLTexture2DTestFixture(const char* vert_src, const char* frag_src, int w, int h)
        : width(w), height(h), shader_prog(vert_src, frag_src)
    {
        SDL_Init(SDL_INIT_VIDEO);
        window = SDL_CreateWindow(
            "Offscreen",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            w, h * 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN
        );
        glctx = SDL_GL_CreateContext(window);
        glewInit();

        REQUIRE(shader_prog.initialize());
        prog = shader_prog.get_program();

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        float quad[] = {
            -1, -1,
             1, -1,
            -1,  1,
             1, -1,
             1,  1,
            -1,  1,
        };
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    }

    ~GLTexture2DTestFixture() {
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
        glDeleteFramebuffers(1, &fbo);
        SDL_GL_DeleteContext(glctx);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
};

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
        GLTexture2DTestFixture fixture(vert_src, frag_src, 256, 1);

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
        REQUIRE(output_param.initialize(fixture.fbo, &fixture.shader_prog));
        REQUIRE(output_param.bind());

        glViewport(0, 0, 256, 1);
        glUseProgram(fixture.prog);
        glBindFramebuffer(GL_FRAMEBUFFER, fixture.fbo);

        output_param.render();

        GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0 + output_param.get_color_attachment()};
        glDrawBuffers(1, drawBuffers);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindVertexArray(fixture.vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

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
        GLTexture2DTestFixture fixture(vert_src, frag_src, 64, 4);

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
        REQUIRE(output_param.initialize(fixture.fbo, &fixture.shader_prog));
        REQUIRE(output_param.bind());

        glViewport(0, 0, 64, 4);
        glUseProgram(fixture.prog);
        glBindFramebuffer(GL_FRAMEBUFFER, fixture.fbo);

        output_param.render();

        GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0 + output_param.get_color_attachment()};
        glDrawBuffers(1, drawBuffers);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindVertexArray(fixture.vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

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