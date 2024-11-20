#pragma once
#ifndef AUDIO_GENERATOR_H
#define AUDIO_GENERATOR_H

#define MAX_TEXTURE_SIZE 4096

#include "audio_render_stage/audio_render_stage.h"

/**
 * @class AudioGenerator
 * @brief The AudioGenerator class is responsible for generating audio data for the shader DSP.
 *
 * The AudioGenerator class provides functionality to generate audio data for the shader DSP. It
 * manages the audio buffer, sample rate, and audio data size. Subclasses can override the
 * `load_audio_data` function to load specific audio data.
 */
class AudioGeneratorRenderStage : public AudioRenderStage {
public:
    /**
     * @brief Constructs an AudioGenerator object.
     * 
     * This constructor initializes the AudioGenerator object with the specified frames per buffer,
     * sample rate, number of channels, and audio file path.
     * 
     * @param frames_per_buffer The number of frames per buffer.
     * @param sample_rate The sample rate of the audio data.
     * @param num_channels The number of channels in the audio data.
     */
    AudioGeneratorRenderStage(const unsigned int frames_per_buffer,
                              const unsigned int sample_rate,
                              const unsigned int num_channels,
                              const char * fragment_shader_path,
                              const std::vector<std::string> & frag_shader_imports = 
                                    {"build/shaders/global_settings.glsl",
                                     "build/shaders/frag_shader_settings.glsl",
                                     "build/shaders/generator_render_stage_settings.glsl"});

    /**
     * @brief Destroys the AudioGenerator object.
     */
    ~AudioGeneratorRenderStage() {}
};

#endif