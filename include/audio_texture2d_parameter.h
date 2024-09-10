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
                          AudioRenderStage * render_stage_linked,
                          GLuint parameter_width,
                          GLuint parameter_height,
                          GLuint datatype = GL_FLOAT,
                          GLuint format = GL_RED,
                          GLuint internal_format = GL_R32F
                          )
        : AudioParameter(name, connection_type, render_stage_linked),
        m_parameter_width(parameter_width),
        m_parameter_height(parameter_height),
        m_datatype(datatype),
        m_format(format),
        m_internal_format(internal_format)
    {};

    ~AudioTexture2DParameter() {
        if (m_texture != 0) {
            glDeleteTextures(1, &m_texture);
        }
        delete[] static_cast<const float *>(m_value);
    }

    bool init() override;

    void set_value(const void * value_ptr) override;

    void update_shader() override;

    GLuint get_texture() const { return m_texture; }

private:
    GLuint m_texture;
    const GLuint m_parameter_width;
    const GLuint m_parameter_height;
    const GLuint m_datatype;
    const GLuint m_format;
    const GLuint m_internal_format;

    static const float FLAT_COLOR[4];
};

#endif // AUDIO_TEXTURE_PARAMETER_H