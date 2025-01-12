#pragma once
#ifndef AUDIO_FINAL_RENDER_STAGE_H
#define AUDIO_FINAL_RENDER_STAGE_H

#include "audio_render_stage/audio_render_stage.h"

/**
 * @class AudioFinalRenderStage
 * @brief The AudioFinalRenderStage class is responsible for the final render stage of the audio renderer.
 * 
 * The AudioFinalRenderStage class provides functionality to render the final stage of the audio renderer,
 * This includes outputting the final buffer output and to the screen if needed
 */

class AudioRenderer;
class AudioFinalRenderStage : public AudioRenderStage {
public:
    friend class AudioRenderer;
    /**
     * @brief Constructs an AudioFinalRenderStage object.
     * 
     * This constructor initializes the AudioFinalRenderStage object with the specified frames per buffer,
     * sample rate, and number of channels.
     * 
     * @param frames_per_buffer The number of frames per buffer.
     * @param sample_rate The sample rate of the audio data.
     * @param num_channels The number of channels in the audio data.
     */
    AudioFinalRenderStage(const unsigned int frames_per_buffer,
                          const unsigned int sample_rate,
                          const unsigned int num_channels);

    /**
     * @brief Destroys the AudioFinalRenderStage object.
     */
    ~AudioFinalRenderStage() {}

    const float * get_output_buffer_data() const { return m_output_buffer_data; }

private:
    /**
     * @brief Overrides the render_render_stage function to provide the rendering functionality.
     * 
     * This function is responsible for rendering the final stage of the audio renderer.
     */
    void render() override;

    bool initialize() override;

    GLuint m_PBO;
    float * m_output_buffer_data;
};

#endif