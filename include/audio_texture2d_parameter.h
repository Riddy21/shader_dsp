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
                          GLuint datatype = GL_FLOAT,
                          GLuint format = GL_RED,
                          GLuint internal_format = GL_R32F
                          )
        : AudioParameter(name, connection_type),
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
    }

    // Getters
    GLuint get_texture() const { return m_texture; }

private:
    class ParamFloatArrayData : public ParamData {
    public:
        ParamFloatArrayData(unsigned int size)
                : m_data(new float[size]()),
                  m_size(size) {}
        ~ParamFloatArrayData() override { delete[] m_data; }
        void * get_data() const override { return m_data; }
        size_t get_size() const override { return sizeof(float) * m_size; }
    private:
        float * m_data;
        int m_size;
    };

    bool initialize_parameter() override;

    void render_parameter() override;

    bool bind_parameter() override;

    std::unique_ptr<ParamData> create_param_data() override {
        return std::make_unique<ParamFloatArrayData>(m_parameter_width * m_parameter_height);
    }

    GLuint m_texture;
    const GLuint m_parameter_width;
    const GLuint m_parameter_height;
    const GLint m_datatype;
    const GLuint m_format;
    const GLuint m_internal_format;

    static const float FLAT_COLOR[4];
};

#endif // AUDIO_TEXTURE_PARAMETER_H