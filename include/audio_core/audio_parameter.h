#pragma once
#ifndef AUDIO_PARAMETER_H
#define AUDIO_PARAMETER_H

#include <GL/glut.h>
#include <GL/freeglut.h>
#include <memory>
#include <vector>

#include "audio_core/audio_param_data.h"
#include "utilities/shader_program.h"

// TODO: Look into making required arguments const for all classes

class AudioRenderStage; // Forward declaration
class AudioRenderer;

class AudioParameter {
public:
    friend class AudioRenderStage;
    friend class AudioRenderer;
    enum ConnectionType {
        INPUT,
        PASSTHROUGH,
        OUTPUT,
        INITIALIZATION
    };

    const std::string name;
    const ConnectionType connection_type;

    // Linking to other parameters
    virtual bool link(AudioParameter * parameter) {
        // Set up bidirectional linking
        if (parameter != nullptr) {
            m_linked_parameter = parameter;
            // Set the reverse link (previous parameter)
            parameter->m_previous_parameter = this;
        }
        return true;
    }

    virtual bool unlink() {
        // Remove bidirectional linking
        if (m_linked_parameter != nullptr) {
            m_linked_parameter->m_previous_parameter = nullptr;
            m_linked_parameter = nullptr;
        }
        if (m_previous_parameter != nullptr) {
            m_previous_parameter->m_linked_parameter = nullptr;
            m_previous_parameter = nullptr;
        }
        return true;
    }

    ~AudioParameter() {
    }

    // Setters
    bool set_value(const void * value_ptr);

    bool set_value(const bool value) {
        return set_value(&value);
    }

    bool set_value(const int value) {
        return set_value(&value);
    }

    bool set_value(const unsigned int value) {
        return set_value(&value);
    }

    bool set_value(const float value) {
        return set_value(&value);
    }

    // Getters
    virtual const void * const get_value() const {
        return m_data->get_data();
    }

    virtual void clear_value() {}

    AudioParameter * get_linked_parameter() const {
        return m_linked_parameter;
    }

    AudioParameter * get_previous_parameter() const {
        return m_previous_parameter;
    }

    GLuint get_framebuffer_linked() const {
        return m_framebuffer_linked;
    }

    bool is_connected() const {
        return m_linked_parameter != nullptr;
    }

    bool has_previous() const {
        return m_previous_parameter != nullptr;
    }


protected:
    AudioParameter(const std::string name,
                   ConnectionType connection_type)
        : name(name),    
          connection_type(connection_type)
    {};

    // Standard functions for parameter
    virtual bool initialize(GLuint frame_buffer=0, AudioShaderProgram * shader_program=nullptr) = 0;

    virtual bool bind() = 0;
    
    virtual bool unbind() = 0;

    virtual void render() = 0;

    virtual std::unique_ptr<ParamData> create_param_data() = 0;

    std::unique_ptr<ParamData> m_data = nullptr; // Using unique pointer to cast to derived class
    AudioParameter * m_linked_parameter = nullptr;
    AudioParameter * m_previous_parameter = nullptr; // Reverse link for bidirectional traversal
    GLuint m_framebuffer_linked = 0;
    AudioShaderProgram * m_shader_program_linked = nullptr;
    bool m_update_param = true;

    AudioParameter(const AudioParameter&) = delete;    // Disable copy and assignment
    AudioParameter& operator=(const AudioParameter&) = delete;



};

#endif // AUDIO_PARAMETER_H