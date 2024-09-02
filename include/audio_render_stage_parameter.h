#pragma once
#ifndef AUDIO_RENDER_STAGE_PARAMETER
#define AUDIO_RENDER_STAGE_PARAMTER

#include <GL/glew.h>

enum ParameterType {
    INITIALIZATION,
    STREAM_INPUT,
    STREAM_PASSTHROUGH,
    STREAM_OUTPUT
};

enum ParameterDataType {
    FLOAT = GL_FLOAT,
    INT = GL_INT,
    UINT,
    BOOL,
    VEC2,
    VEC3,
    VEC4,
    MAT2,
    MAT3,
    MAT4,
    SAMPLER2D,
    SAMPLER3D,
    SAMPLERCUBE
};

struct AudioRenderStageParameter {
    ParameterType type;
    ParameterDataType datatype;

    // Initialization parameters
    unsigned int parameter_size;

};

#endif