#pragma once
#ifndef AUDIO_RENDER_STAGE_H
#define AUDIO_RENDER_STAGE_H

#include <vector>
#include <GL/glew.h>

class AudioRenderStage {
private:
    // Shader source
    GLuint shader_program; // Keeps a copy of the shader program associated with the stage
    const GLchar* fragment_source = R"glsl(
        #version 300 es
        precision highp float;

        in vec2 TexCoord;

        uniform sampler2D input_audio_texture;
        uniform sampler2D stream_audio_texture;

        out vec4 outColor;

        void main() {
            outColor = mix(texture(input_audio_texture, TexCoord),
                           texture(stream_audio_texture, TexCoord), 0.5);
        }
    )glsl"; // Default shader source (Kept in files for the future versions)

public:
    friend class AudioRenderer; // Allow AudioRenderer to access private members

    // Settings
    const unsigned int frames_per_buffer;
    const unsigned int sample_rate;
    const unsigned int num_channels;

    // Audio Data
    // Initilize with 0 vector
    std::vector<float> audio_buffer = std::vector<float>(frames_per_buffer * num_channels, 0.0f);

    // #TODO: Add linkages to next stage
};

#endif // AUDIO_RENDER_STAGE_H