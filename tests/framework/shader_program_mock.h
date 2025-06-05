#pragma once
#ifndef SHADER_PROGRAM_MOCK_H
#define SHADER_PROGRAM_MOCK_H

#include <GL/glew.h>
#include <string>
#include "utilities/shader_program.h"
#include "test_mock.h"

/**
 * @brief Mock implementation of AudioShaderProgram for testing
 * 
 * This class mocks the shader program functionality needed for audio parameter tests
 * without requiring a real OpenGL shader program compilation.
 */
class AudioShaderProgramMock : public AudioShaderProgram {
public:
    AudioShaderProgramMock() 
        : AudioShaderProgram("", ""),  // Call the parent constructor with empty strings
          m_mockProgram(1) {}          // Non-zero to simulate valid program

    ~AudioShaderProgramMock() = default;

    /**
     * @brief Mock implementation of shader program initialization
     * 
     * @return True if initialization succeeded, false otherwise
     */
    bool initialize() {
        return true;
    }

    /**
     * @brief Get the program ID (override parent's implementation)
     * 
     * @return The mock shader program ID
     */
    GLuint get_program() const {
        return m_mockProgram;
    }

    /**
     * @brief Set custom fragment shader source for testing
     * 
     * @param source The fragment shader source to set
     */
    void setFragmentShaderSource(const std::string& source) {
        m_mockFragmentSource = source;
    }

    /**
     * @brief Set custom vertex shader source for testing
     * 
     * @param source The vertex shader source to set
     */
    void setVertexShaderSource(const std::string& source) {
        m_mockVertexSource = source;
    }

    /**
     * @brief Override the parent's get_fragment_shader_source to use our mock source
     */
    const std::string get_fragment_shader_source() const {
        return m_mockFragmentSource.empty() ? 
            AudioShaderProgram::get_fragment_shader_source() : m_mockFragmentSource;
    }

    /**
     * @brief Override the parent's get_vertex_shader_source to use our mock source
     */
    const std::string get_vertex_shader_source() const {
        return m_mockVertexSource.empty() ? 
            AudioShaderProgram::get_vertex_shader_source() : m_mockVertexSource;
    }

    /**
     * @brief Set the mock program ID
     * 
     * @param programId The program ID to set
     */
    void setProgramId(GLuint programId) {
        m_mockProgram = programId;
    }

private:
    GLuint m_mockProgram;
    std::string m_mockVertexSource;
    std::string m_mockFragmentSource;
};

#endif // SHADER_PROGRAM_MOCK_H