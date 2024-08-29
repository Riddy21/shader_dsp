#pragma once
#ifndef AUDIO_RENDER_STAGE_H
#define AUDIO_RENDER_STAGE_H

#include <vector>
#include <GL/glew.h>

class AudioRenderStage {
private:
    // Shader source
    GLuint shader_program; // Keeps a copy of the shader program associated with the stage

public:
    friend class AudioRenderer; // Allow AudioRenderer to access private members
    // Constructor
    AudioRenderStage(const unsigned int frames_per_buffer,
                     const unsigned int sample_rate,
                     const unsigned int num_channels)
        : m_frames_per_buffer(frames_per_buffer),
          m_sample_rate(sample_rate),
          m_num_channels(num_channels) {}

    /**
     * @brief Loads audio data into the specified buffer index.
     * 
     * This function is responsible for loading audio data into the buffer specified by the buffer_index parameter.
     * 
     * @param buffer_index The index of the buffer to load the audio data into.
     */
    virtual void update() {};

    // Settings
    const unsigned int m_frames_per_buffer;
    const unsigned int m_sample_rate;
    const unsigned int m_num_channels;

    // Audio Data
    // Initilize with 0 vector
    const float * m_audio_buffer = new float[m_frames_per_buffer * m_num_channels]();

    const GLchar* m_fragment_source = R"glsl(
        #version 300 es
        precision highp float;

        in vec2 TexCoord;

        uniform sampler2D input_audio_texture;
        uniform sampler2D stream_audio_texture;

        out vec4 FragColor;

        void main() {
            FragColor = texture(input_audio_texture, TexCoord) +
                        texture(stream_audio_texture, TexCoord);
        }
    )glsl"; // Default shader source (Kept in files for the future versions)

    // #TODO: Add linkages to next stage
};

#endif // AUDIO_RENDER_STAGE_H