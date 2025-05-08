#pragma once
#ifndef AUDIO_UNIFORM_ARRAY_PARAMETERS_H
#define AUDIO_UNIFORM_ARRAY_PARAMETERS_H

#include <GL/glew.h>
#include <GL/glut.h>
#include <vector>
#include "audio_core/audio_parameter.h"
#include "audio_parameter/audio_uniform_parameter.h"

class AudioUniformArrayParameter : public AudioUniformParameter {

protected:
    AudioUniformArrayParameter(const std::string name,
                               AudioParameter::ConnectionType connection_type,
                               size_t array_size)
        : AudioUniformParameter(name, connection_type), m_array_size(array_size) {}
    ~AudioUniformArrayParameter() = default;

    size_t m_array_size;

};

class AudioIntArrayParameter : public AudioUniformArrayParameter {
public:
    AudioIntArrayParameter(const std::string name,
                           AudioParameter::ConnectionType connection_type,
                           size_t array_size)
        : AudioUniformArrayParameter(name, connection_type, array_size)
    {};
    ~AudioIntArrayParameter() = default;
private:
    std::unique_ptr<ParamData> create_param_data() override {
        return std::make_unique<ParamIntArrayData>(m_array_size);
    }

    void set_uniform(GLint location) override {
        glUniform1iv(location, m_array_size, static_cast<int *>(m_data->get_data()));
    }
};

class AudioFloatArrayParameter : public AudioUniformArrayParameter {
public:
    AudioFloatArrayParameter(const std::string name,
                             AudioParameter::ConnectionType connection_type,
                             size_t array_size)
        : AudioUniformArrayParameter(name, connection_type, array_size)
    {};
    ~AudioFloatArrayParameter() = default;
private:
    std::unique_ptr<ParamData> create_param_data() override {
        return std::make_unique<ParamFloatArrayData>(m_array_size);
    }

    void set_uniform(GLint location) override {
        glUniform1fv(location, m_array_size, static_cast<float *>(m_data->get_data()));
    }
};

class AudioBoolArrayParameter : public AudioUniformArrayParameter {
public:
    AudioBoolArrayParameter(const std::string name,
                            AudioParameter::ConnectionType connection_type,
                            size_t array_size)
        : AudioUniformArrayParameter(name, connection_type, array_size)
    {};
    ~AudioBoolArrayParameter() = default;
private:
    std::unique_ptr<ParamData> create_param_data() override {
        return std::make_unique<ParamBoolArrayData>(m_array_size);
    }

    void set_uniform(GLint location) override {
        std::vector<int> bool_data(m_array_size);
        for (size_t i = 0; i < m_array_size; ++i) {
            bool_data[i] = static_cast<bool *>(m_data->get_data())[i];
        }
        glUniform1iv(location, m_array_size, bool_data.data());
    }
};

#endif // AUDIO_UNIFORM_ARRAY_PARAMETERS_H
