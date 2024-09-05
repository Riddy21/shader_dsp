#pragma once
#ifndef AUDIO_RENDER_STAGE_PARAMETER
#define AUDIO_RENDER_STAGE_PARAMTER

#include <GL/glew.h>

class AudioRenderStageParameter {

public:
    enum Type {
        INITIALIZATION,
        STREAM_INPUT,
        STREAM_OUTPUT
    };
    
    enum DataType {
        FLOAT = GL_FLOAT,
        INT = GL_INT,
        UINT = GL_UNSIGNED_INT,
        BOOL = GL_BOOL,
        VEC2 = GL_FLOAT_VEC2,
        VEC3 = GL_FLOAT_VEC3,
        VEC4 = GL_FLOAT_VEC4,
        MAT2 = GL_FLOAT_MAT2,
        MAT3 = GL_FLOAT_MAT3,
        MAT4 = GL_FLOAT_MAT4
    };
    
    enum Format {
        RED = GL_RED,
        RG = GL_RG,
        RGB = GL_RGB,
        RGBA = GL_RGBA
    };
    
    enum InternalFormat {
        R8 = GL_R8,
        RG8 = GL_RG8,
        RGB8 = GL_RGB8,
        RGBA8 = GL_RGBA8,
        R16F = GL_R16F,
        RG16F = GL_RG16F,
        RGB16F = GL_RGB16F,
        RGBA16F = GL_RGBA16F,
        R32F = GL_R32F,
        RG32F = GL_RG32F,
        RGB32F = GL_RGB32F,
        RGBA32F = GL_RGBA32F
    };

    // FIXME: Reoganize this with a proper constructor and destructor
    const char * name = nullptr;
    const char * link_name = nullptr;
    Type type = Type::INITIALIZATION;
    DataType datatype = DataType::FLOAT;
    Format format = Format::RED;
    InternalFormat internal_format = InternalFormat::R32F;
    unsigned int parameter_width = 0;
    unsigned int parameter_height = 0;
    const void * data = nullptr;
    bool is_bound = false;

    GLuint framebuffer = 0;
    GLuint texture = 0;

    static GLuint generate_framebuffer(AudioRenderStageParameter & parameter);
    static GLuint generate_texture(AudioRenderStageParameter & parameter);
    static void bind_framebuffer_to_texture(AudioRenderStageParameter & output_parameter,
                                            AudioRenderStageParameter & input_parameter);

private:
    static GLuint color_attachment_index;
};

#endif