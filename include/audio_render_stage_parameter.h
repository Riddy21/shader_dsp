#pragma once
#ifndef AUDIO_RENDER_STAGE_PARAMETER
#define AUDIO_RENDER_STAGE_PARAMTER

#include <GL/glew.h>

enum ParameterType {
    // FIXME: Figure out how to do passthroughs
    INITIALIZATION,
    STREAM_INPUT,
    STREAM_OUTPUT
};

enum ParameterDataType {
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

enum ParameterFormat {
    RED = GL_RED,
    RG = GL_RG,
    RGB = GL_RGB,
    RGBA = GL_RGBA
};

enum ParameterInternalFormat {
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

struct AudioRenderStageParameter {
    const char * name;
    ParameterType type;
    ParameterDataType datatype;
    ParameterFormat format;
    ParameterInternalFormat internal_format;
    unsigned int parameter_width;
    unsigned int parameter_height;
    const void * data;
    // Add links to textures and stuff
};

#endif