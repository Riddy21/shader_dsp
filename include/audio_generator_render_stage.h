#pragma once
#ifndef AUDIO_GENERATOR_H
#define AUDIO_GENERATOR_H

#include "audio_render_stage.h"

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
     * @param audio_filepath The path to the audio file to use
     */
    AudioGeneratorRenderStage(const unsigned int frames_per_buffer,
                              const unsigned int sample_rate,
                              const unsigned int num_channels,
                              const char * audio_filepath);

    /**
     * @brief Destroys the AudioGenerator object.
     */
    ~AudioGeneratorRenderStage() {} // TODO: Implement destructor

    /**
     * @brief Updates the audio data in the specified buffer index.
     * 
     * This function is responsible for updating the audio data in the specified buffer index.
     */
    void update();

    /**
     * @brief Plays the audio data.
     * 
     * This function is responsible for playing the audio data.
     */
    void play();

    /**
     * @brief Stops the audio data.
     * 
     * This function is responsible for stopping the audio data.
     */
    void stop();

    const char * m_audio_filepath; // Default audio file path

private:
    // Load audio file
    /**
     * @brief Loads audio data from the specified audio file.
     * 
     * This function is responsible for loading audio data from the specified audio file into the full_audio_data vector.
     */
    static std::vector<float> load_audio_data_from_file(const char * audio_filepath);

    // controls
    bool m_is_playing = false;
    unsigned int m_play_index = 0;
    // Audio data
    std::vector<float> m_full_audio_data;
    // Empty audio sound data
    std::vector<float> m_empty_audio_data = std::vector<float>(m_frames_per_buffer * m_num_channels, 0.0f);
};

#endif