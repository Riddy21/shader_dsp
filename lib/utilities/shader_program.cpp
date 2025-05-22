#include <iostream>

#include "utilities/shader_program.h"

AudioShaderProgram::AudioShaderProgram(const std::string& vertex_shader_source, const std::string& fragment_shader_source)
    : m_vertex_shader_source(vertex_shader_source), m_fragment_shader_source(fragment_shader_source),
      m_vertex_shader(0), m_fragment_shader(0), m_shader_program(0) {}

AudioShaderProgram::~AudioShaderProgram() {
    glDeleteProgram(m_shader_program);
    glDeleteShader(m_vertex_shader);
    glDeleteShader(m_fragment_shader);
}

bool AudioShaderProgram::initialize() {
    m_vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    m_fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    if (!compile_shader(m_vertex_shader, m_vertex_shader_source)) {
        return false;
    }

    if (!compile_shader(m_fragment_shader, m_fragment_shader_source)) {
        return false;
    }

    m_shader_program = glCreateProgram();
    glAttachShader(m_shader_program, m_vertex_shader);
    glAttachShader(m_shader_program, m_fragment_shader);

    return link_program();
}

GLuint AudioShaderProgram::get_program() const {
    return m_shader_program;
}

bool AudioShaderProgram::compile_shader(GLuint shader, const std::string& source) {
    const GLchar* shader_source = source.c_str();
    glShaderSource(shader, 1, &shader_source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar info_log[512];
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        std::cerr << "Error compiling shader: " << info_log << std::endl;
        std::cerr << "Shader source: " << source << std::endl;
        return false;
    }
    return true;
}

bool AudioShaderProgram::link_program() {
    glLinkProgram(m_shader_program);

    GLint success;
    glGetProgramiv(m_shader_program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar info_log[512];
        glGetProgramInfoLog(m_shader_program, 512, NULL, info_log);
        std::cerr << "Error linking shader program: " << info_log << std::endl;
        return false;
    }
    return true;
}