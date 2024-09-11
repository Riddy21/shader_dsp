#pragma once
#ifndef AUDIO_PARAMETER_H
#define AUDIO_PARAMETER_H

#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <memory>
#include <vector>

#include "audio_render_stage.h"

class AudioRenderStage; // Forward declaration

class AudioParameter {
public:
    enum ConnectionType {
        INPUT,
        PASSTHROUGH,
        OUTPUT,
        INITIALIZATION
    };

    const char * name;
    const ConnectionType connection_type;

    ~AudioParameter() {
        //delete m_deleter;
    }

    virtual bool init() = 0;

    virtual void set_value(const void * value_ptr) = 0;

    const void * const get_value() const {
        return m_value;
    }

    virtual void render_parameter() = 0;

    void link_render_stage(const AudioRenderStage * render_stage) {
        m_render_stage_linked = render_stage;
    }

protected:
    struct ParamData {
        virtual ~ParamData() = default;
        virtual void * get_data() const = 0;
    };

    AudioParameter(const char * name,
                   ConnectionType connection_type)
        : name(name),    
          connection_type(connection_type)
    {};

    std::unique_ptr<ParamData> m_data = nullptr;
    void * m_value = nullptr;
    const AudioRenderStage * m_render_stage_linked = nullptr;
};


#endif // AUDIO_PARAMETER_H