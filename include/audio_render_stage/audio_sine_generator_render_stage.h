#pragma once
#ifndef AUDIO_SINE_GENERATOR_RENDER_STAGE_H
#define AUDIO_SINE_GENERATOR_RENDER_STAGE_H

#include "audio_render_stage/audio_generator_render_stage.h"

/**
 * @class AudioSineGeneratorRenderStage
 * @brief The AudioSineGeneratorRenderStage class is responsible for generating audio data from a sine wave.
 * 
 */
class AudioSineGeneratorRenderStage : public AudioGeneratorRenderStage {
public:
    /**
     * @brief Constructs an AudioSineGeneratorRenderStage object.
     * 
     * This constructor initializes the AudioSineGeneratorRenderStage object with the specified frames per buffer,
     * sample rate, number of channels, the fragment shader linked
     * 
     * @param frames_per_buffer The number of frames per buffer.
     * @param sample_rate The sample rate of the audio data.
     * @param num_channels The number of channels in the audio data.
     */
    AudioSineGeneratorRenderStage(const unsigned int frames_per_buffer,
                                  const unsigned int sample_rate,
                                  const unsigned int num_channels)
        : AudioGeneratorRenderStage(frames_per_buffer, sample_rate, num_channels, "build/shaders/sine_generator_render_stage.frag") {}

    /**
     * @brief Destroys the AudioSineGeneratorRenderStage object.
     */
    ~AudioSineGeneratorRenderStage() {}
};

#endif