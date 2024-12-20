#pragma once
#ifndef AUDIO_TEXTURE_PARAMETER_H
#define AUDIO_TEXTURE_PARAMETER_H

#include <GL/glew.h>
#include <GL/glut.h>

#include "audio_parameter.h"

class AudioTexture2DParameter : public AudioParameter {
public:
    AudioTexture2DParameter(const char * name,
                          AudioParameter::ConnectionType connection_type,
                          GLuint parameter_width,
                          GLuint parameter_height,
                          GLuint active_texture = 0, // For Inputs
                          GLuint color_attachment = 0, // For outputs
                          GLuint datatype = GL_FLOAT,
                          GLuint format = GL_RED,
                          GLuint internal_format = GL_R32F
                          )
        : AudioParameter(name, connection_type),
        m_parameter_width(parameter_width),
        m_parameter_height(parameter_height),
        m_active_texture(active_texture),
        m_color_attachment(color_attachment),
        m_datatype(datatype),
        m_format(format),
        m_internal_format(internal_format)
    {};

    ~AudioTexture2DParameter() {
        if (m_texture != 0) {
            glDeleteTextures(1, &m_texture);
        }
    }

    // Getters
    GLuint get_texture() const { return m_texture; }

private:

    bool initialize_parameter() override;

    void render_parameter() override;

    bool bind_parameter() override;

    std::unique_ptr<ParamData> create_param_data() override {
        return std::make_unique<ParamFloatArrayData>(m_parameter_width * m_parameter_height);
    }

    GLuint m_texture;
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