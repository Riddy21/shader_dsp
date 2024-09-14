#pragma once
#ifndef AUDIO_INT_PARAMETER_H
#define AUDIO_INT_PARAMETER_H

#include <GL/glew.h>
#include <GL/glut.h>

#include "audio_parameter.h"

class AudioIntParameter : public AudioParameter {

public:
    AudioIntParameter(const char * name,
                      AudioParameter::ConnectionType connection_type)
        : AudioParameter(name, connection_type),
          m_binding_point(total_binding_points++)
    {};
    ~AudioIntParameter() = default;

    // Setters
    bool set_value(const void * value_ptr) override;

private:
    class ParamIntData : public ParamData {
    public:
        ParamIntData()
                : m_data(0) {}
        ~ParamIntData() override = default;
        void * get_data() const override { return (void *) &m_data; }
    private:
        int m_data;
    };

    bool initialize_parameter() override;

    void render_parameter() override;

    bool bind_parameter() override;

    GLuint m_ubo;

    const unsigned int m_binding_point = 0;

    static unsigned int total_binding_points;

};

#endif // AUDIO_INT_PARAMETER_H