#pragma once
#ifndef AUDIO_UNIFORM_BUFFER_PARAMETERS_H   
#define AUDIO_UNIFORM_BUFFER_PARAMETERS_H

#include <GL/glew.h>
#include <GL/glut.h>

#include "audio_core/audio_parameter.h"

class AudioUniformBufferParameter : public AudioParameter {

protected:
    AudioUniformBufferParameter(const std::string name,
                      AudioParameter::ConnectionType connection_type);
    ~AudioUniformBufferParameter() = default;

private:
    bool initialize(GLuint frame_buffer=0, AudioShaderProgram * shader_program=nullptr) override;

    void render() override;

    bool bind() override {return true;}
    
    bool unbind() override {return true;}

    GLuint m_ubo;

    const unsigned int m_binding_point = 0;

    static unsigned int total_binding_points;

};

class AudioIntBufferParameter : public AudioUniformBufferParameter {
public:
    AudioIntBufferParameter(const std::string name,
                      AudioParameter::ConnectionType connection_type)
        : AudioUniformBufferParameter(name, connection_type)
    {};
    ~AudioIntBufferParameter() = default;

private:
    std::unique_ptr<ParamData> create_param_data() override {
        return std::make_unique<ParamIntData>();
    }
};

class AudioFloatBufferParameter : public AudioUniformBufferParameter {
public:
    AudioFloatBufferParameter(const std::string name,
                        AudioParameter::ConnectionType connection_type)
        : AudioUniformBufferParameter(name, connection_type)
    {};
    ~AudioFloatBufferParameter() = default;

private:
    std::unique_ptr<ParamData> create_param_data() override {
        return std::make_unique<ParamFloatData>();
    }
};

class AudioBoolBufferParameter : public AudioUniformBufferParameter {
public:
    AudioBoolBufferParameter(const std::string name,
                       AudioParameter::ConnectionType connection_type)
        : AudioUniformBufferParameter(name, connection_type)
    {};
    ~AudioBoolBufferParameter() = default;

private:

    std::unique_ptr<ParamData> create_param_data() override {
        return std::make_unique<ParamBoolData>();
    }
};

#endif // AUDIO_UNIFORM_BUFFER_PARAMETERS_H