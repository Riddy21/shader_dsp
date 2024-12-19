#pragma once
#ifndef AUDIO_PARAMETER_H
#define AUDIO_PARAMETER_H

#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <memory>
#include <vector>

#include "audio_parameter/audio_param_data.h"
#include "audio_core/audio_renderer.h"
#include "audio_render_stage/audio_render_stage.h"

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

    ~AudioParameter() {
        printf("Deleting parameter %s\n", name.c_str());
    }

    // Linking to other parameters
    // TODO: Make these private
    virtual bool link(AudioParameter * parameter) {
        m_linked_parameter = parameter;
        return true;
    }

    virtual AudioParameter * unlink() {
        AudioParameter * linked = m_linked_parameter;
        m_linked_parameter = nullptr;
        return linked;
    }

    // Setters
    virtual bool set_value(const void * value_ptr);

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
    const void * const get_value() const {
        return m_data->get_data();
    }

    AudioRenderStage * get_linked_render_stage() const {
        return m_render_stage_linked;
    }

    AudioParameter * get_linked_parameter() const {
        return m_linked_parameter;
    }

    bool is_connected() const {
        return m_linked_parameter != nullptr;
    }


protected:
    AudioParameter(const std::string name,
                   ConnectionType connection_type)
        : name(name),    
          connection_type(connection_type)
    {};

    virtual bool initialize_parameter() = 0;

    virtual bool bind_parameter() = 0;

    virtual void render_parameter() = 0;

    virtual std::unique_ptr<ParamData> create_param_data() = 0;

    void link_render_stage(AudioRenderStage * render_stage) {
        m_render_stage_linked = render_stage;
    }

    std::unique_ptr<ParamData> m_data = nullptr; // Using unique pointer to cast to derived class
    AudioRenderStage * m_render_stage_linked = nullptr;
    AudioParameter * m_linked_parameter = nullptr;
};


#endif // AUDIO_PARAMETER_H