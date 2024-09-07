#pragma once
#ifndef AUDIO_RENDER_STAGE_PARAMETER
#define AUDIO_RENDER_STAGE_PARAMTER

#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut.h>

class AudioRenderStageParameter {

public:
    enum Type {
        INITIALIZATION,
        STREAM_INPUT,
        STREAM_OUTPUT
    };

    AudioRenderStageParameter() = default;

    AudioRenderStageParameter(const char * name,
                              unsigned int parameter_width,
                              unsigned int parameter_height,
                              const float ** data = nullptr,
                              const char * link_name = nullptr,
                              Type type = Type::INITIALIZATION,
                              GLuint datatype = GL_FLOAT,
                              GLuint format = GL_RED,
                              GLuint internal_format = GL_R32F)
        : name(name),
          link_name(link_name),
          type(type),
          datatype(datatype),
          format(format),
          internal_format(internal_format),
          parameter_width(parameter_width),
          parameter_height(parameter_height),
          data(data)
    {};
    
    // FIXME: Reoganize this with a proper constructor and destructor
    const char * name = nullptr;
    const char * link_name = nullptr;
    Type type = Type::INITIALIZATION;
    GLuint datatype = GL_FLOAT;
    GLuint format = GL_RED;
    GLuint internal_format = GL_R32F;
    unsigned int parameter_width = 0;
    unsigned int parameter_height = 0;
    const float ** data = nullptr;
    bool is_bound = false;

    GLuint framebuffer = 0;
    GLuint texture = 0;

    static GLuint color_attachment_index;

    static GLuint generate_texture(AudioRenderStageParameter & parameter);
    static void bind_framebuffer_to_texture(AudioRenderStageParameter & output_parameter,
                                            AudioRenderStageParameter & input_parameter);
    static void bind_framebuffer_to_output(AudioRenderStageParameter & output_parameter);
};

#endif