#pragma once
#ifndef AUDIO_PARAMETER_H
#define AUDIO_PARAMETER_H

#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <memory>
#include <vector>

#include "audio_render_stage.h"
#include "audio_param_data.h"

class AudioRenderStage; // Forward declaration

class AudioParameter {
public:
    friend class AudioRenderStage;
    enum ConnectionType {
        INPUT,
        PASSTHROUGH,
        OUTPUT,
        INITIALIZATION
    };

    const char * name;
    const ConnectionType connection_type;

    ~AudioParameter() {
        printf("Deleting parameter %s\n", name);
    }

    // Linking to other parameters
    virtual bool link(const AudioParameter * parameter) {
        m_linked_parameter = parameter;
        return true;
    }

    // Setters
    virtual bool set_value(const void * value_ptr);

    // Getters
    const void * const get_value() const {
        return m_data->get_data();
    }

    bool is_connected() const {
        return m_linked_parameter != nullptr;
    }

protected:
    AudioParameter(const char * name,
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
    const AudioParameter * m_linked_parameter = nullptr;
};


#endif // AUDIO_PARAMETER_H