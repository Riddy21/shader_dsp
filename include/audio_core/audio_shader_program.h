#pragma once
#ifndef AUDIO_SHADER_PROGRAM_H
#define AUDIO_SHADER_PROGRAM_H

#include <GL/glew.h>
#include <string>

class AudioShaderProgram {
public:
    AudioShaderProgram(const std::string& vertex_shader_source, const std::string& fragment_shader_source);
    ~AudioShaderProgram();

    bool initialize();
    GLuint get_program() const;
    const std::string get_vertex_shader_source() const {return m_vertex_shader_source;}
    const std::string get_fragment_shader_source() const {return m_fragment_shader_source;}

private:
    bool compile_shader(GLuint shader, const std::string& source);
    bool link_program();

    GLuint m_vertex_shader;
    GLuint m_fragment_shader;
    GLuint m_shader_program;

    std::string m_vertex_shader_source;
    std::string m_fragment_shader_source;
};

#endif // AUDIO_SHADER_PROGRAM_H