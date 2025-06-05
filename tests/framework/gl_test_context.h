#pragma once
#ifndef GL_TEST_CONTEXT_H
#define GL_TEST_CONTEXT_H

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <memory>
#include <stdexcept>
#include <iostream>

/**
 * @brief GLTestContext creates and manages a minimal OpenGL context for testing
 * 
 * This class sets up a minimal OpenGL context using SDL2 for testing purposes.
 * It creates a hidden window and OpenGL context that can be used for tests that
 * require OpenGL functionality without displaying anything on screen.
 */
class GLTestContext {
public:
    /**
     * @brief Get the singleton instance of the GLTestContext
     * 
     * @return Reference to the singleton instance
     */
    static GLTestContext& getInstance() {
        static GLTestContext instance;
        return instance;
    }

    /**
     * @brief Initialize the OpenGL context
     * 
     * Creates a hidden SDL window and initializes an OpenGL context.
     * 
     * @return True if initialization was successful, false otherwise
     */
    bool initialize() {
        if (m_initialized) {
            return true;
        }

        // Initialize SDL for video
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
            return false;
        }

        // Set OpenGL attributes
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

        // Create hidden window
        m_window = SDL_CreateWindow(
            "Test GL Context",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            1, 1,
            SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN
        );

        if (!m_window) {
            std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
            SDL_Quit();
            return false;
        }

        // Create OpenGL context
        m_glContext = SDL_GL_CreateContext(m_window);
        if (!m_glContext) {
            std::cerr << "Failed to create GL context: " << SDL_GetError() << std::endl;
            SDL_DestroyWindow(m_window);
            SDL_Quit();
            return false;
        }

        // Make the context current
        SDL_GL_MakeCurrent(m_window, m_glContext);

        // Initialize GLEW
        glewExperimental = GL_TRUE;
        GLenum glewError = glewInit();
        if (glewError != GLEW_OK) {
            std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(glewError) << std::endl;
            SDL_GL_DeleteContext(m_glContext);
            SDL_DestroyWindow(m_window);
            SDL_Quit();
            return false;
        }

        // Check for any OpenGL errors
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "OpenGL error during init: " << error << std::endl;
            // Continue anyway, some GL implementations may set errors after glewInit
        }

        m_initialized = true;
        return true;
    }

    /**
     * @brief Clean up OpenGL context and SDL resources
     */
    void cleanup() {
        if (m_glContext) {
            SDL_GL_DeleteContext(m_glContext);
            m_glContext = nullptr;
        }

        if (m_window) {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }

        SDL_Quit();
        m_initialized = false;
    }

    /**
     * @brief Check if the context is initialized
     * 
     * @return True if initialized, false otherwise
     */
    bool isInitialized() const {
        return m_initialized;
    }

    /**
     * @brief Make this OpenGL context current for the calling thread
     */
    void makeCurrent() {
        if (!m_initialized) {
            throw std::runtime_error("Attempting to make uninitialized GL context current");
        }
        SDL_GL_MakeCurrent(m_window, m_glContext);
    }

    /**
     * @brief Get the SDL window
     * 
     * @return SDL_Window pointer
     */
    SDL_Window* getWindow() const {
        return m_window;
    }

    /**
     * @brief Get the OpenGL context
     * 
     * @return SDL_GLContext object
     */
    SDL_GLContext getContext() const {
        return m_glContext;
    }

private:
    GLTestContext() : m_window(nullptr), m_glContext(nullptr), m_initialized(false) {}
    
    ~GLTestContext() {
        cleanup();
    }
    
    // Delete copy constructor and assignment operator
    GLTestContext(const GLTestContext&) = delete;
    GLTestContext& operator=(const GLTestContext&) = delete;

    SDL_Window* m_window;
    SDL_GLContext m_glContext;
    bool m_initialized;
};

/**
 * @brief Fixture class for tests requiring OpenGL context
 * 
 * This class initializes and cleans up the OpenGL context for test cases.
 * It should be used as a test fixture in tests that require OpenGL functionality.
 */
class GLTestFixture {
public:
    GLTestFixture() {
        // Initialize GL context before each test
        if (!GLTestContext::getInstance().initialize()) {
            throw std::runtime_error("Failed to initialize GL test context");
        }
    }

    ~GLTestFixture() {
        // Cleanup not needed here since the singleton manages it
        // Just make sure GL errors are cleared
        while (glGetError() != GL_NO_ERROR) {}
    }
};

#endif // GL_TEST_CONTEXT_H