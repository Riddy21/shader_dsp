#include <SDL2/SDL.h>
#include <GL/glew.h>
#include "utilities/shader_program.h"
#include "catch2/catch_all.hpp"


// Helper fixture for OpenGL/texture test setup

struct SDLWindow {
    SDL_Window * window = nullptr;
    SDL_GLContext glctx = nullptr;
    int width, height;

    SDLWindow(int w, int h)
        : width(w), height(h) {
        SDL_Init(SDL_INIT_VIDEO);
        window = SDL_CreateWindow(
            "Offscreen",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN
        );
        glctx = SDL_GL_CreateContext(window);
        glewInit();
    }

    ~SDLWindow() {
        SDL_GL_DeleteContext(glctx);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
};
struct GLFramebuffer {
    GLuint fbo = 0;

    GLFramebuffer() {
        glGenFramebuffers(1, &fbo);
    }

    ~GLFramebuffer() {
        glDeleteFramebuffers(1, &fbo);
    }

    void bind() {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    }

    void unbind() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
};

struct GLContext {
    GLuint vao = 0, vbo = 0;
    AudioShaderProgram shader_prog;
    GLuint prog;

    GLContext(const char * vert_src, const char * frag_src)
        : shader_prog(vert_src, frag_src)
    {
        REQUIRE(shader_prog.initialize());
        prog = shader_prog.get_program();

        float quad[] = {
            // Position    Texcoords
            -1.0f, -1.0f, 0.0f, 0.0f,  // Bottom-left
            -1.0f,  1.0f, 0.0f, 1.0f,  // Top-left
             1.0f, -1.0f, 1.0f, 0.0f,  // Bottom-right
             1.0f,  1.0f, 1.0f, 1.0f,  // Top-right
            -1.0f,  1.0f, 0.0f, 1.0f,  // Top-left
             1.0f, -1.0f, 1.0f, 0.0f   // Bottom-right
        };

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)0); // Vertex attributes
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)(2 * sizeof(GLfloat))); // Texture attributes
        glEnableVertexAttribArray(1);

        // Unbind the buffers
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        // Check for OpenGL errors
        GLenum error = glGetError();
        REQUIRE(error == GL_NO_ERROR);
    }

    ~GLContext() {
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
    }

    void pre_draw() {
        glUseProgram(prog);
    }

    void draw(GLenum * drawBuffers) {
        glDrawBuffers(1, drawBuffers);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
};
