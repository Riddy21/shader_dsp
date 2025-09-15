#pragma once
#ifndef AUDIO_UNIFORM_BUFFER_PARAMETERS_H   
#define AUDIO_UNIFORM_BUFFER_PARAMETERS_H

#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <unordered_map>

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

    struct ContextData {
        std::unordered_map<std::string, unsigned> binding_points;
        unsigned next_binding_point = 0;
    };

    static std::unordered_map<void*, ContextData> s_context_registry;

    static ContextData &current_context_data();

    // Returns the binding point that should be used for the given block name
    // *for the current GL context*. A fresh binding point is allocated the
    // first time a block-name is requested inside a context.
    static unsigned get_binding_point_for_block(const std::string &name);

    // Bind every uniform block that has been registered **in the current GL
    // context** to its remembered binding point for this program.
public:
    static void bind_registered_blocks(GLuint program);

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