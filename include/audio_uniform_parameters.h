#pragma once
#ifndef AUDIO_UNIFORM_PARAMETERS_H
#define AUDIO_UNIFORM_PARAMETERS_H

#include <GL/glew.h>
#include <GL/glut.h>

#include "audio_parameter.h"

class AudioUniformParameter : public AudioParameter {

protected:
    AudioUniformParameter(const char * name,
                          AudioParameter::ConnectionType connection_type);
    ~AudioUniformParameter() = default;

private:
    bool initialize_parameter() override {
        return true;
    }

    void render_parameter() override;

    bool bind_parameter() override {
        return true;
    }

    virtual void set_uniform(GLint location) = 0;

    bool m_initialized = false;
};

class AudioIntParameter : public AudioUniformParameter {
public:
    AudioIntParameter(const char * name,
                      AudioParameter::ConnectionType connection_type)
        : AudioUniformParameter(name, connection_type)
    {};
    ~AudioIntParameter() = default;
private:
    std::unique_ptr<ParamData> create_param_data() override {
        return std::make_unique<ParamIntData>();
    }

    void set_uniform(GLint location) override {
        glUniform1i(location, *static_cast<int *>(m_data->get_data()));
    }
};

class AudioFloatParameter : public AudioUniformParameter {
public:
    AudioFloatParameter(const char * name,
                        AudioParameter::ConnectionType connection_type)
        : AudioUniformParameter(name, connection_type)
    {};
    ~AudioFloatParameter() = default;
private:
    std::unique_ptr<ParamData> create_param_data() override {
        return std::make_unique<ParamFloatData>();
    }

    void set_uniform(GLint location) override {
        glUniform1f(location, *static_cast<float *>(m_data->get_data()));
        printf("Setting uniform %s to %f\n", name, *static_cast<float *>(m_data->get_data()));
    }
};

class AudioBoolParameter : public AudioUniformParameter {
public:
    AudioBoolParameter(const char * name,
                       AudioParameter::ConnectionType connection_type)
        : AudioUniformParameter(name, connection_type)
    {};
    ~AudioBoolParameter() = default;
private:
    std::unique_ptr<ParamData> create_param_data() override {
        return std::make_unique<ParamBoolData>();
    }

    void set_uniform(GLint location) override {
        glUniform1i(location, *static_cast<bool *>(m_data->get_data()));
    }
};

#endif // AUDIO_UNIFORM_PARAMETERS_H