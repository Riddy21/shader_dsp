#pragma once
#ifndef AUDIO_RENDER_STAGE_H
#define AUDIO_RENDER_STAGE_H

#include <vector>
#include <unordered_map>
#include <GL/glew.h>

#include "audio_render_stage_parameter.h"

class AudioRenderStage {
private:
    // Shader source
    GLuint m_shader_program; // Keeps a copy of the shader program associated with the stage

    // Framebuffer for the stage if it involves outputs
    GLuint m_framebuffer;

    /**
     * @brief Compile the shader program.
     * 
     * This function is responsible for compiling the shader program.
     */
    bool compile_shader_program();

    /**
     * @brief Compile parameters
     * 
     * This function prepared the framebuffers and textures for the parameters
     */
    bool compile_parameters();

    /**
     * @brief Initialize the framebuffer.
     * 
     * This function is responsible for initializing the framebuffer.
     */
    bool initialize_framebuffer();

    // Parameters
    std::vector<AudioRenderStageParameter> m_parameters;

public:
    friend class AudioRenderer;
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

    /** 
     * @brief Add a parameter to the audio parameter list
     * 
     * This function adds a parameter to the audio parameter list
     * 
     * @param parameter The parameter to add
     * @return True if the parameter is successfully added, false otherwise.
     */
    bool add_parameter(AudioRenderStageParameter & parameter);

    /**
     * @brief Returns the parameters of the render stage.
     * 
     * This function returns the parameters of the render stage with the type specified
     * 
     * @return The parameters of the render stage.
     */
    std::unordered_map<const char *, AudioRenderStageParameter &> get_parameters_with_type(AudioRenderStageParameter::Type type);

    /**
     * @brief Link this stage to another stage
     * 
     * This function links this render stage to another stage by linking frame buffers and textures of 2 stages
     */
    static bool link_stages(AudioRenderStage & stage1, AudioRenderStage & stage2);

    // Settings
    const unsigned int m_frames_per_buffer;
    const unsigned int m_sample_rate;
    const unsigned int m_num_channels;

    // Audio Data
    // Initilize with 0 vector
    const float * m_audio_buffer = new float[m_frames_per_buffer * m_num_channels]();

    // TODO: Make this a private in the future
    GLchar const * m_fragment_source = R"glsl(
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

};

#endif // AUDIO_RENDER_STAGE_H