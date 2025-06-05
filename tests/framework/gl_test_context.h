#pragma once
#ifndef GL_TEST_CONTEXT_H
#define GL_TEST_CONTEXT_H

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <string>
#include <functional>

#include "utilities/shader_program.h"

/**
 * @brief A reusable OpenGL test context for audio renderer and parameter tests
 * 
 * This class provides a minimal OpenGL environment for testing components
 * that rely on OpenGL functionality without requiring a full application setup.
 * It can be used for testing audio parameters, render stages, and the renderer itself.
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
     * @brief Initialize the OpenGL context with default settings
     * 
     * @return True if initialization was successful, false otherwise
     */
    bool initialize() {
        return initialize(512, 44100, 2); // Default audio settings
    }

    /**
     * @brief Initialize the OpenGL context with specific audio settings
     * 
     * @param bufferSize The audio buffer size to use for test resources
     * @param sampleRate The audio sample rate to use for test resources
     * @param numChannels The number of audio channels to use for test resources
     * @return True if initialization was successful, false otherwise
     */
    bool initialize(unsigned int bufferSize, unsigned int sampleRate, unsigned int numChannels) {
        if (m_initialized) {
            return true;
        }

        m_bufferSize = bufferSize;
        m_sampleRate = sampleRate;
        m_numChannels = numChannels;

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
            1, 1,  // Minimal size
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

        // Clear any potential errors from GLEW initialization
        while (glGetError() != GL_NO_ERROR) {}

        // Initialize test resources
        createTestResources();

        m_initialized = true;
        return true;
    }

    /**
     * @brief Clean up OpenGL context and SDL resources
     */
    void cleanup() {
        cleanupTestResources();
        
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
     * @brief Make this OpenGL context current for the calling thread
     */
    void makeCurrent() {
        if (!m_initialized) {
            throw std::runtime_error("Attempting to make uninitialized GL context current");
        }
        SDL_GL_MakeCurrent(m_window, m_glContext);
    }

    /**
     * @brief Run a function with the GL context made current
     * 
     * This function ensures the GL context is current during the execution
     * of the provided function and handles any GL errors that occur.
     * 
     * @param func Function to execute with GL context current
     * @return True if the function executed without GL errors, false otherwise
     */
    bool withContext(const std::function<void()>& func) {
        if (!m_initialized && !initialize()) {
            return false;
        }
        
        makeCurrent();
        
        // Clear any previous GL errors
        while (glGetError() != GL_NO_ERROR) {}
        
        // Execute the function
        func();
        
        // Check for GL errors
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "GL error occurred: " << error << std::endl;
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Initialize a parameter with the test context
     * 
     * @param parameter The parameter to initialize
     * @return True if initialization succeeded, false otherwise
     */
    bool initializeParameter(AudioParameter* parameter) {
        return withContext([&]() {
            parameter->initialize(m_framebuffer, m_shaderProgram.get());
        });
    }

    /**
     * @brief Get a framebuffer for testing
     * 
     * @return GLuint framebuffer ID
     */
    GLuint getFramebuffer() const {
        return m_framebuffer;
    }
    
    /**
     * @brief Get the shader program for testing
     * 
     * @return Pointer to the shader program
     */
    AudioShaderProgram* getShaderProgram() {
        return m_shaderProgram.get();
    }
    
    /**
     * @brief Set custom vertex/fragment shader sources
     * 
     * Use this to configure the test shader program with specific
     * shader code needed for particular tests.
     * 
     * @param vertexSource Vertex shader source code
     * @param fragmentSource Fragment shader source code
     * @return True if shader compilation succeeded, false otherwise
     */
    bool setShaderSources(const std::string& vertexSource, const std::string& fragmentSource) {
        if (m_shaderProgram) {
            m_shaderProgram.reset();
        }
        
        m_shaderProgram = std::make_unique<AudioShaderProgram>(vertexSource, fragmentSource);
        return m_shaderProgram->initialize();
    }
    
    /**
     * @brief Create a new framebuffer for testing
     * 
     * @param frameBufferId Pointer to store the new framebuffer ID
     * @return True if creation succeeded, false otherwise
     */
    bool createFramebuffer(GLuint* frameBufferId) {
        return withContext([&]() {
            glGenFramebuffers(1, frameBufferId);
        });
    }
    
    /**
     * @brief Delete a framebuffer
     * 
     * @param frameBufferId The framebuffer ID to delete
     * @return True if deletion succeeded, false otherwise
     */
    bool deleteFramebuffer(GLuint frameBufferId) {
        return withContext([&]() {
            glDeleteFramebuffers(1, &frameBufferId);
        });
    }
    
    /**
     * @brief Get the buffer size used for test setup
     * 
     * @return The buffer size
     */
    unsigned int getBufferSize() const {
        return m_bufferSize;
    }
    
    /**
     * @brief Get the sample rate used for test setup
     * 
     * @return The sample rate
     */
    unsigned int getSampleRate() const {
        return m_sampleRate;
    }
    
    /**
     * @brief Get the number of channels used for test setup
     * 
     * @return The number of channels
     */
    unsigned int getNumChannels() const {
        return m_numChannels;
    }
    
    /**
     * @brief Create an audio buffer with test data
     * 
     * @return Unique pointer to a float array with test data
     */
    std::unique_ptr<float[]> createTestAudioBuffer() {
        const size_t size = m_bufferSize * m_numChannels;
        auto buffer = std::make_unique<float[]>(size);
        
        // Generate a simple sine wave pattern
        for (size_t i = 0; i < size; ++i) {
            buffer[i] = 0.5f * std::sin(static_cast<float>(i) * 0.01f);
        }
        
        return buffer;
    }
    
    /**
     * @brief Check if the context has been initialized
     * 
     * @return True if initialized, false otherwise
     */
    bool isInitialized() const {
        return m_initialized;
    }

private:
    GLTestContext() 
        : m_window(nullptr), 
          m_glContext(nullptr), 
          m_initialized(false),
          m_framebuffer(0),
          m_bufferSize(512),
          m_sampleRate(44100),
          m_numChannels(2) {}
    
    ~GLTestContext() {
        cleanup();
    }
    
    // Delete copy constructor and assignment operator
    GLTestContext(const GLTestContext&) = delete;
    GLTestContext& operator=(const GLTestContext&) = delete;
    
    /**
     * @brief Create resources needed for testing
     */
    void createTestResources() {
        // Create framebuffer
        glGenFramebuffers(1, &m_framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
        
        // Create basic vertex and fragment shaders with common parameter names
        const std::string vertexShaderSource = 
            "#version 330 core\n"
            "layout(location = 0) in vec3 position;\n"
            "void main() {\n"
            "    gl_Position = vec4(position, 1.0);\n"
            "}\n";
            
        const std::string fragmentShaderSource = 
            "#version 330 core\n"
            // Common parameter names used in tests
            "uniform sampler2D textureParam;\n"
            "uniform sampler2D stream_audio_texture;\n"
            "uniform sampler2D output_audio_texture;\n"
            "uniform float time;\n"
            "uniform int frame;\n"
            "out vec4 outputColor;\n"
            "void main() {\n"
            "    outputColor = texture(textureParam, vec2(0.0));\n"
            "}\n";
        
        m_shaderProgram = std::make_unique<AudioShaderProgram>(vertexShaderSource, fragmentShaderSource);
        m_shaderProgram->initialize();
        
        // Create and configure VAO/VBO for rendering if needed
        createQuadResources();
    }
    
    /**
     * @brief Create and configure quad resources for rendering
     */
    void createQuadResources() {
        // Create VAO
        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);
        
        // Create VBO with quad vertices
        glGenBuffers(1, &m_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        
        // Simple quad vertices for full-screen rendering
        float vertices[] = {
            // positions
            -1.0f, -1.0f, 0.0f,
             1.0f, -1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f, 0.0f
        };
        
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        // Configure vertex attribute
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        
        // Unbind
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    
    /**
     * @brief Clean up test resources
     */
    void cleanupTestResources() {
        if (m_vbo) {
            glDeleteBuffers(1, &m_vbo);
            m_vbo = 0;
        }
        
        if (m_vao) {
            glDeleteVertexArrays(1, &m_vao);
            m_vao = 0;
        }
        
        if (m_framebuffer) {
            glDeleteFramebuffers(1, &m_framebuffer);
            m_framebuffer = 0;
        }
        
        m_shaderProgram.reset();
    }

    SDL_Window* m_window;
    SDL_GLContext m_glContext;
    bool m_initialized;
    
    GLuint m_framebuffer;
    GLuint m_vao;
    GLuint m_vbo;
    std::unique_ptr<AudioShaderProgram> m_shaderProgram;
    
    // Audio settings
    unsigned int m_bufferSize;
    unsigned int m_sampleRate;
    unsigned int m_numChannels;
};

/**
 * @brief Fixture class for tests requiring OpenGL context
 * 
 * Use this fixture in CATCH2 test cases that need OpenGL functionality.
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
    
    /**
     * @brief Initialize with specific audio settings
     * 
     * @param bufferSize Buffer size for audio
     * @param sampleRate Sample rate for audio
     * @param numChannels Number of audio channels
     * @return True if initialization succeeded
     */
    bool initializeWithAudioSettings(unsigned int bufferSize, unsigned int sampleRate, unsigned int numChannels) {
        return GLTestContext::getInstance().initialize(bufferSize, sampleRate, numChannels);
    }

    /**
     * @brief Run a function with the GL context
     * 
     * @param func Function to execute with GL context current
     * @return True if the function executed without GL errors
     */
    bool withContext(const std::function<void()>& func) {
        return GLTestContext::getInstance().withContext(func);
    }
    
    /**
     * @brief Initialize a parameter with the test context
     * 
     * @param parameter The parameter to initialize
     * @return True if initialization succeeded
     */
    bool initializeParameter(AudioParameter* parameter) {
        return GLTestContext::getInstance().initializeParameter(parameter);
    }
    
    /**
     * @brief Get the framebuffer used by the test context
     * 
     * @return GLuint framebuffer ID
     */
    GLuint getFramebuffer() const {
        return GLTestContext::getInstance().getFramebuffer();
    }
    
    /**
     * @brief Get the shader program used by the test context
     * 
     * @return Pointer to the shader program
     */
    AudioShaderProgram* getShaderProgram() {
        return GLTestContext::getInstance().getShaderProgram();
    }
    
    /**
     * @brief Set custom shader sources for the test
     * 
     * @param vertexSource Vertex shader source code
     * @param fragmentSource Fragment shader source code
     * @return True if shader compilation succeeded
     */
    bool setShaderSources(const std::string& vertexSource, const std::string& fragmentSource) {
        return GLTestContext::getInstance().setShaderSources(vertexSource, fragmentSource);
    }
    
    /**
     * @brief Create a test audio buffer
     * 
     * @return Unique pointer to a float array with test data
     */
    std::unique_ptr<float[]> createTestAudioBuffer() {
        return GLTestContext::getInstance().createTestAudioBuffer();
    }
    
    /**
     * @brief Get the buffer size used by the test context
     * 
     * @return The buffer size
     */
    unsigned int getBufferSize() const {
        return GLTestContext::getInstance().getBufferSize();
    }
    
    /**
     * @brief Get the sample rate used by the test context
     * 
     * @return The sample rate
     */
    unsigned int getSampleRate() const {
        return GLTestContext::getInstance().getSampleRate();
    }
    
    /**
     * @brief Get the number of channels used by the test context
     * 
     * @return The number of channels
     */
    unsigned int getNumChannels() const {
        return GLTestContext::getInstance().getNumChannels();
    }
};

#endif // GL_TEST_CONTEXT_H