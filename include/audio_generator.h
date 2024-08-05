#pragma once
#ifndef AUDIO_GENERATOR_H
#define AUDIO_GENERATOR_H

#include <vector>

/**
 * @class AudioGenerator
 * @brief The AudioGenerator class is responsible for generating audio data for the shader DSP.
 *
 * The AudioGenerator class provides functionality to generate audio data for the shader DSP. It
 * manages the audio buffer, sample rate, and audio data size. Subclasses can override the
 * `load_audio_data` function to load specific audio data.
 */
class AudioGenerator {
public:
    /**
     * @brief Constructs an AudioGenerator object with the specified audio data size and sample rate.
     *
     * @param audio_data_size The size of the audio data buffer.
     * @param sample_rate The sample rate of the audio data.
     */
    AudioGenerator(const unsigned audio_data_size, const unsigned sample_rate);

    /**
     * @brief Destroys the AudioGenerator object.
     */
    ~AudioGenerator();

    /**
     * @brief Updates the audio buffer with new audio data.
     *
     * @return True if the audio buffer was successfully updated, false otherwise.
     */
    bool update_audio_buffer();

private:
    /**
     * @brief Sets up the audio buffer and other necessary resources.
     *
     * @return True if the setup was successful, false otherwise.
     */
    bool setup_audio_buffer();

    /**
     * @brief Loads the audio data into the audio buffer.
     *
     * This function should be overridden by subclasses to load specific audio data.
     */
    virtual void load_audio_data();

    const unsigned channels = 2; ///< The number of audio channels.
    const unsigned sample_rate; ///< The sample rate of the audio data.
    const unsigned audio_data_size; ///< The size of the audio data buffer.
    std::vector<float> audio_data_left; ///< The audio data for the left channel.
    std::vector<float> audio_data_right; ///< The audio data for the right channel.
    GLuint input_pixel_buffer; ///< The input pixel buffer.
    GLuint generator_texture; ///< The generator texture.
    GLuint audio_framebuffer; ///< The audio framebuffer.
};

#endif