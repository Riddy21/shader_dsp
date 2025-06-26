#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <iostream>
#include "utilities/shader_program.h"
#include "catch2/catch_all.hpp"


// Helper fixture for OpenGL/texture test setup

struct SDLWindow {
    SDL_Window * window = nullptr;
    SDL_GLContext glctx = nullptr;
    int width, height;
    
    // EGL objects
    EGLDisplay eglDisplay = EGL_NO_DISPLAY;
    EGLSurface eglSurface = EGL_NO_SURFACE;
    EGLContext eglContext = EGL_NO_CONTEXT;
    EGLConfig eglConfig = nullptr;

    SDLWindow(int w, int h)
        : width(w), height(h) {
        SDL_Init(SDL_INIT_VIDEO);
        window = SDL_CreateWindow(
            "Offscreen",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            w, h, SDL_WINDOW_HIDDEN // Remove SDL_WINDOW_OPENGL flag since we're using EGL
        );
        
        // Initialize EGL
        initialize_egl();
        
        // Set dummy context for compatibility
        glctx = (SDL_GLContext)0x1;
    }

    ~SDLWindow() {
        cleanup_egl();
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

private:
    bool initialize_egl() {
        // Get EGL display
        eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (eglDisplay == EGL_NO_DISPLAY) {
            std::cerr << "EGL: Failed to get EGL display" << std::endl;
            return false;
        }

        // Initialize EGL
        EGLint majorVersion, minorVersion;
        if (!eglInitialize(eglDisplay, &majorVersion, &minorVersion)) {
            std::cerr << "EGL: Failed to initialize EGL" << std::endl;
            return false;
        }

        // Choose EGL config
        const EGLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_STENCIL_SIZE, 8,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_NONE
        };

        EGLint numConfigs;
        if (!eglChooseConfig(eglDisplay, configAttribs, &eglConfig, 1, &numConfigs)) {
            std::cerr << "EGL: Failed to choose EGL config" << std::endl;
            return false;
        }

        if (numConfigs == 0) {
            std::cerr << "EGL: No suitable EGL config found" << std::endl;
            return false;
        }

        // Create EGL surface
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
            std::cerr << "EGL: Failed to get window WM info" << std::endl;
            return false;
        }

        eglSurface = eglCreateWindowSurface(eglDisplay, eglConfig, 
                                          (EGLNativeWindowType)wmInfo.info.x11.window, NULL);
        if (eglSurface == EGL_NO_SURFACE) {
            std::cerr << "EGL: Failed to create EGL surface" << std::endl;
            return false;
        }

        // Create EGL context
        const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE
        };

        eglContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttribs);
        if (eglContext == EGL_NO_CONTEXT) {
            std::cerr << "EGL: Failed to create EGL context" << std::endl;
            return false;
        }

        // Make current
        if (!eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
            std::cerr << "EGL: Failed to make context current" << std::endl;
            return false;
        }

        return true;
    }

    void cleanup_egl() {
        if (eglContext != EGL_NO_CONTEXT) {
            eglDestroyContext(eglDisplay, eglContext);
            eglContext = EGL_NO_CONTEXT;
        }
        
        if (eglSurface != EGL_NO_SURFACE) {
            eglDestroySurface(eglDisplay, eglSurface);
            eglSurface = EGL_NO_SURFACE;
        }
        
        if (eglDisplay != EGL_NO_DISPLAY) {
            eglTerminate(eglDisplay);
            eglDisplay = EGL_NO_DISPLAY;
        }
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
