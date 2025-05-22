#pragma once
#ifndef AUDIO_TEXTURE_PARAMETER_H
#define AUDIO_TEXTURE_PARAMETER_H

#include <GL/glew.h>
#include <GL/glut.h>

#include "audio_core/audio_parameter.h"

class AudioTexture2DParameter : public AudioParameter {
public:
    AudioTexture2DParameter(const std::string name,
                          AudioParameter::ConnectionType connection_type,
                          GLuint parameter_width,
                          GLuint parameter_height,
                          GLuint active_texture = 0, // For Inputs
                          GLuint color_attachment = 0, // For outputs
                          GLuint texture_filter_type = GL_NEAREST,
                          GLuint datatype = GL_FLOAT,
                          GLuint format = GL_RED,
                          GLuint internal_format = GL_R32F
                          );

    ~AudioTexture2DParameter() ;

    // Getters
    GLuint get_texture() const { return m_texture; }

    const void * const get_value() const override;

    void clear_value() override;

    GLuint get_color_attachment() const { return m_color_attachment; }

private:

    bool initialize(GLuint frame_buffer=0, AudioShaderProgram * shader_program=nullptr) override;

    void render() override;

    bool bind() override;

    bool unbind() override;

    std::unique_ptr<ParamData> create_param_data() override {
        return std::make_unique<ParamFloatArrayData>(m_parameter_width * m_parameter_height);
    }

    GLuint m_texture;
    GLuint m_PBO;

    const GLuint m_filter_type;
    const GLuint m_parameter_width;
    const GLuint m_parameter_height;
    const GLuint m_active_texture;
    const GLuint m_color_attachment;
    const GLint m_datatype;
    const GLuint m_format;
    const GLuint m_internal_format;

    static const float FLAT_COLOR[4];
};

#endif // AUDIO_TEXTURE_PARAMETER_H