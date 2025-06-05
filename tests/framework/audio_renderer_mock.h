#pragma once
#ifndef AUDIO_RENDERER_MOCK_H
#define AUDIO_RENDERER_MOCK_H

#include "audio_core/audio_renderer.h"
#include "utilities/shader_program.h"
#include "gl_mock.h"
#include <memory>

/**
 * @brief Mock implementation of AudioRenderer for testing purposes
 * 
 * This class provides a test-friendly version of the AudioRenderer with
 * controlled OpenGL context and functionality for testing audio parameters
 * without requiring a full AudioRenderer instance.
 */
class AudioRendererMock {
public:
    /**
     * @brief Get the singleton instance of the mock renderer
     * 
     * @return Reference to the singleton instance
     */
    static AudioRendererMock& getInstance() {
        static AudioRendererMock instance;
        return instance;
    }

    /**
     * @brief Initialize the mock renderer with minimum required resources
     * 
     * @return True if initialization succeeded, false otherwise
     */
    bool initialize() {
        if (m_initialized) {
            return true;
        }

        // Create and configure a minimal framebuffer for tests
        glGenFramebuffers(1, &m_framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

        // Create a test shader program with minimal shader sources
        const std::string vertexShaderSource = 
            "#version 330 core\n"
            "layout(location = 0) in vec3 position;\n"
            "void main() {\n"
            "    gl_Position = vec4(position, 1.0);\n"
            "}\n";
            
        // Set up a minimal fragment shader with expected uniform/texture parameters
        const std::string fragmentShaderSource = 
            "#version 330 core\n"
            "uniform sampler2D textureParam;\n"     // Texture parameter used in tests
            "out vec4 outputColor;\n"               // Example output
            "void main() {\n"
            "    outputColor = texture(textureParam, vec2(0.0));\n"
            "}\n";
            
        m_shaderProgram = std::make_unique<AudioShaderProgram>(vertexShaderSource, fragmentShaderSource);
        bool success = m_shaderProgram->initialize();
        if (!success) {
            cleanup();
            return false;
        }
        
        m_initialized = true;
        return true;
    }

    /**
     * @brief Clean up resources
     */
    void cleanup() {
        if (m_framebuffer) {
            glDeleteFramebuffers(1, &m_framebuffer);
            m_framebuffer = 0;
        }
        
        m_shaderProgram.reset();
        m_initialized = false;
    }

    /**
     * @brief Get the framebuffer used for testing
     * 
     * @return The framebuffer ID
     */
    GLuint getFramebuffer() const {
        return m_framebuffer;
    }

    /**
     * @brief Get the shader program used for testing
     * 
     * @return Pointer to the shader program
     */
    AudioShaderProgram* getShaderProgram() {
        return m_shaderProgram.get();
    }

    /**
     * @brief Initialize a test parameter
     * 
     * Provides a controlled environment for parameter initialization
     * 
     * @param parameter The parameter to initialize
     * @return True if initialization succeeded, false otherwise
     */
    bool initializeParameter(AudioParameter* parameter) {
        if (!m_initialized) {
            if (!initialize()) {
                return false;
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
        bool result = parameter->initialize(m_framebuffer, m_shaderProgram.get());
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        return result;
    }

    /**
     * @brief Cleanup a test parameter
     * 
     * Helps ensure parameters are properly cleaned up
     * 
     * @param parameter The parameter to clean up
     */
    void cleanupParameter(AudioParameter* parameter) {
        // Parameters handle their own cleanup, but might need context
        // For now, just ensure the context is available
        if (m_initialized) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }

    /**
     * @brief Get the buffer size used for test parameters
     * 
     * @return The buffer size
     */
    unsigned int getBufferSize() const {
        return 512; // Common test buffer size
    }

    /**
     * @brief Get the sample rate used for test parameters
     * 
     * @return The sample rate
     */
    unsigned int getSampleRate() const {
        return 44100; // Common test sample rate
    }

    /**
     * @brief Get the number of channels used for test parameters
     * 
     * @return The number of channels
     */
    unsigned int getNumChannels() const {
        return 2; // Stereo testing
    }

    /**
     * @brief Check if the renderer is initialized
     * 
     * @return True if initialized, false otherwise
     */
    bool isInitialized() const {
        return m_initialized;
    }

private:
    AudioRendererMock() : m_framebuffer(0), m_initialized(false) {}
    
    ~AudioRendererMock() {
        cleanup();
    }
    
    // Delete copy constructor and assignment operator
    AudioRendererMock(const AudioRendererMock&) = delete;
    AudioRendererMock& operator=(const AudioRendererMock&) = delete;

    GLuint m_framebuffer;
    std::unique_ptr<AudioShaderProgram> m_shaderProgram;
    bool m_initialized;
};

/**
 * @brief Fixture class for tests requiring audio parameter setup with OpenGL
 * 
 * This class initializes both the GL context and mock audio renderer for test cases.
 */
class AudioParameterTestFixture {
public:
    AudioParameterTestFixture() {
        // Set up GL mocks
        GL_MOCK_SETUP();
        
        // Initialize the mock renderer
        if (!AudioRendererMock::getInstance().initialize()) {
            throw std::runtime_error("Failed to initialize mock renderer");
        }
    }

    ~AudioParameterTestFixture() {
        // Reset mocks
        Mock::reset();
    }

    /**
     * @brief Helper to initialize a parameter for testing
     * 
     * @param parameter The parameter to initialize
     * @return True if initialization succeeded, false otherwise
     */
    bool initializeParameter(AudioParameter* parameter) {
        return AudioRendererMock::getInstance().initializeParameter(parameter);
    }

    /**
     * @brief Get the mock renderer
     * 
     * @return Reference to the mock renderer
     */
    AudioRendererMock& getRenderer() {
        return AudioRendererMock::getInstance();
    }
};

#endif // AUDIO_RENDERER_MOCK_H