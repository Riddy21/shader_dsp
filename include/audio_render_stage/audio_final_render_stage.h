#pragma once
#ifndef AUDIO_FINAL_RENDER_STAGE_H
#define AUDIO_FINAL_RENDER_STAGE_H

#include "audio_core/audio_render_stage.h"

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
                          const unsigned int num_channels,
                          const std::string& fragment_shader_path = "build/shaders/final_render_stage.glsl",
                          const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports);

    static const std::vector<std::string> default_frag_shader_imports;

    /**
     * @brief Destroys the AudioFinalRenderStage object.
     */
    ~AudioFinalRenderStage() {}

    const std::vector<float> & get_output_buffer_data() const { return m_output_buffer_data; }
    
    const std::vector<std::vector<float>> & get_output_data_channel_seperated() const { return m_output_data_channel_seperated; }

private:
    /**
     * @brief Overrides the render_render_stage function to provide the rendering functionality.
     * 
     * This function is responsible for rendering the final stage of the audio renderer.
     */
    void render(unsigned int time) override;

    std::vector<float> m_output_buffer_data;

    std::vector<std::vector<float>> m_output_data_channel_seperated;
};

#endif