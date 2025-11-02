#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <iostream>
#include <vector>
#include "utilities/shader_program.h"
#include "catch2/catch_all.hpp"


// Helper fixture for OpenGL/texture test setup

struct SDLWindow {
    SDL_Window * window = nullptr;
    SDL_GLContext glctx = nullptr;
    int width, height;
    bool visible;
    
    // EGL objects
    EGLDisplay eglDisplay = EGL_NO_DISPLAY;
    EGLSurface eglSurface = EGL_NO_SURFACE;
    EGLContext eglContext = EGL_NO_CONTEXT;
    EGLConfig eglConfig = nullptr;

    // Constructor for hidden window (default, for offscreen rendering)
    SDLWindow(int w, int h)
        : width(w), height(h), visible(false) {
        window = SDL_CreateWindow(
            "Offscreen",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            w, h, SDL_WINDOW_HIDDEN
        );
        
        // Initialize EGL
        initialize_egl();
    }
    
    // Constructor for visible window (for visualization/debugging)
    SDLWindow(int w, int h, const char* title, bool make_visible = true)
        : width(w), height(h), visible(make_visible) {
        Uint32 flags = make_visible ? SDL_WINDOW_SHOWN : SDL_WINDOW_HIDDEN;
        window = SDL_CreateWindow(
            title ? title : "Test Window",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            w, h, flags
        );
        
        if (!window) {
            std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
            return;
        }
        
        // Initialize EGL
        initialize_egl();
    }

    ~SDLWindow() {
        cleanup_egl();
        if (window) {
            SDL_DestroyWindow(window);
        }
    }
    
    // Swap buffers for visible windows
    void swap_buffers() {
        if (eglDisplay != EGL_NO_DISPLAY && eglSurface != EGL_NO_SURFACE) {
            eglSwapBuffers(eglDisplay, eglSurface);
        }
    }
    
    // Get window pointer (for event handling, etc.)
    SDL_Window* get_window() const {
        return window;
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

    GLContext()
    {
        float quad[] = {
            // Position    Texcoords (flipped vertically)
            -1.0f, -1.0f, 0.0f, 1.0f,  // Bottom-left
            -1.0f,  1.0f, 0.0f, 0.0f,  // Top-left
             1.0f, -1.0f, 1.0f, 1.0f,  // Bottom-right
             1.0f,  1.0f, 1.0f, 0.0f,  // Top-right
            -1.0f,  1.0f, 0.0f, 0.0f,  // Top-left
             1.0f, -1.0f, 1.0f, 1.0f   // Bottom-right
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

        // Set GL settings for audio rendering
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        // GL_FRAMEBUFFER_SRGB is not available in GLES3, skip it
    }

    ~GLContext() {
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
    }

    void prepare_draw() {
        glBindVertexArray(vao);
    }

    void set_draw_buffers(const std::vector<GLenum>& drawBuffers) {
        glDrawBuffers(drawBuffers.size(), drawBuffers.data());
    }

    void draw() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
};
