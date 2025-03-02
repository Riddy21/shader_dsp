#pragma once
#ifndef AUDIO_GENERATOR_H
#define AUDIO_GENERATOR_H

#include "audio_render_stage/audio_render_stage.h"

#define MIDDLE_C 261.63f
#define SEMI_TONE 1.059463f

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
    static const std::vector<std::string> default_frag_shader_imports;

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
                              const std::string& fragment_shader_path,
                              const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports);

    /**
     * @brief Destroys the AudioGenerator object.
     */
    ~AudioGeneratorRenderStage() {}
};

#endif