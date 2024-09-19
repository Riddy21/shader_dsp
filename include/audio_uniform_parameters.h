#pragma once
#ifndef AUDIO_UNIFORM_PARAMETERS_H   
#define AUDIO_UNIFORM_PARAMETERS_H

#include <GL/glew.h>
#include <GL/glut.h>

#include "audio_parameter.h"

class AudioUniformParameter : public AudioParameter {

protected:
    AudioUniformParameter(const char * name,
                      AudioParameter::ConnectionType connection_type)
        : AudioParameter(name, connection_type),
          m_binding_point(total_binding_points++)
    {};
    ~AudioUniformParameter() = default;

private:
    bool initialize_parameter() override;

    void render_parameter() override;

    bool bind_parameter() override;

    GLuint m_ubo;

    const unsigned int m_binding_point = 0;

    static unsigned int total_binding_points;

};

class AudioIntParameter : public AudioUniformParameter {
public:
    AudioIntParameter(const char * name,
                      AudioParameter::ConnectionType connection_type)
        : AudioUniformParameter(name, connection_type)
    {};
    ~AudioIntParameter() = default;

private:
    class ParamIntData : public ParamData {
    public:
        ParamIntData()
                : m_data(0) {}
        ~ParamIntData() override = default;
        void * get_data() const override { return (void *) &m_data; }
        size_t get_size() const override { return sizeof(int); }
    private:
        int m_data;
    };

    std::unique_ptr<ParamData> create_param_data() override {
        return std::make_unique<ParamIntData>();
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
    class ParamFloatData : public ParamData {
    public:
        ParamFloatData()
                : m_data(0.0f) {}
        ~ParamFloatData() override = default;
        void * get_data() const override { return (void *) &m_data; }
        size_t get_size() const override { return sizeof(float); }
    private:
        float m_data;
    };

    std::unique_ptr<ParamData> create_param_data() override {
        return std::make_unique<ParamFloatData>();
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
    class ParamBoolData : public ParamData {
    public:
        ParamBoolData()
                : m_data(false) {}
        ~ParamBoolData() override = default;
        void * get_data() const override { return (void *) &m_data; }
        size_t get_size() const override { return sizeof(bool); }
    private:
        bool m_data;
    };

    std::unique_ptr<ParamData> create_param_data() override {
        return std::make_unique<ParamBoolData>();
    }
};

#endif // AUDIO_UNIFORM_PARAMETERS_H