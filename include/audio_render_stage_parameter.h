#pragma once
#ifndef AUDIO_RENDER_STAGE_PARAMETER
#define AUDIO_RENDER_STAGE_PARAMTER

#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut.h>

class AudioRenderStageParameter {
public:
    friend class AudioRenderStage;
    enum Type {
        INITIALIZATION,
        STREAM_INPUT,
        STREAM_OUTPUT
    };

    AudioRenderStageParameter(const char * name,
                              Type type,
                              unsigned int parameter_width,
                              unsigned int parameter_height,
                              const float ** data = nullptr,
                              const char * link_name = nullptr,
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

    ~AudioRenderStageParameter() {
        if (m_texture != 0) {
            glDeleteTextures(1, &m_texture);
        }
    }
    
    const char * name = nullptr;
    const char * link_name = nullptr;
    const Type type = Type::INITIALIZATION;
    const GLuint datatype = GL_FLOAT;
    const GLuint format = GL_RED;
    const GLuint internal_format = GL_R32F;
    const unsigned int parameter_width = 0;
    const unsigned int parameter_height = 0;
    const float ** data = nullptr;

    GLuint get_texture() const {
        return m_texture;
    }

    GLuint get_framebuffer() const {
        return m_framebuffer;
    }

    bool is_bound() const {
        return m_is_bound;
    }

    static GLuint get_latest_color_attachment_index() {
        return m_color_attachment_index;
    }

    static void bind_framebuffer_to_texture(AudioRenderStageParameter & output_parameter,
                                            AudioRenderStageParameter & input_parameter);
    static void bind_framebuffer_to_output(AudioRenderStageParameter & output_parameter);

private:
    void generate_texture();

    static GLuint m_color_attachment_index;

    GLuint m_texture = 0;
    GLuint m_framebuffer  = 0;
    bool m_is_bound = false;
};

#endif