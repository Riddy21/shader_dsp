#pragma once
#ifndef AUDIO_PARAMETER_H
#define AUDIO_PARAMETER_H

#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <memory>
#include <vector>

#include "audio_render_stage.h"

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

    ~AudioParameter() {};

    virtual bool init() = 0;

    virtual void set_value(const void * value_ptr) = 0;

    virtual void update_shader() = 0;

protected:
    AudioParameter(const char * name,
                   ConnectionType connection_type,
                   const AudioRenderStage * render_stage_linked)
        : name(name),    
          connection_type(connection_type),
          m_render_stage_linked(render_stage_linked)
    {};

    void * m_value = nullptr;
    const AudioRenderStage * m_render_stage_linked;
};


#endif // AUDIO_PARAMETER_H