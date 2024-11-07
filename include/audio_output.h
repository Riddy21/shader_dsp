#pragma once
#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include <memory>
#include "audio_buffer.h"
#include "audio_swap_buffer.h"

/**
 * @class AudioOutput
 * @brief Represents an audio output device.
 *
 * This class provides functionality for audio output, including sample rate, number of channels, and frames per buffer.
 * It also contains a link to an audio buffer for storing audio data.
 */
class AudioOutput {

public:
    /**
     * Constructor for the AudioDriver class.
     * 
     * @param sample_rate The sample rate of the audio stream.
     * @param frames_per_buffer The number of frames per buffer.
     * @param channels The number of channels in the audio stream.
    */
    AudioOutput(const unsigned frames_per_buffer, const unsigned sample_rate, const unsigned channels) : 
        m_sample_rate(sample_rate),
        m_channels(channels),
        m_frames_per_buffer(frames_per_buffer) {}

    /**
     * Destructor for the AudioDriver class.
    */
    ~AudioOutput() {}

    /**
     * Link a buffer to the audio driver
     * 
     * @param buffer_vector The buffer to link to the audio driver.
     * @return True if the buffer was linked successfully, false otherwise.
    */
    bool set_buffer_link(AudioBuffer * buffer) {
        m_audio_buffer_link = buffer;
        return true;
    }

    /**
     * Start the output device
     * 
     * @return True if the output device was started successfully, false otherwise.
     */
    virtual bool start() = 0;
    
    /**
     * Stop the output device
     * 
     * @return True if the output device was stopped successfully, false otherwise.
     */
    virtual bool stop() = 0;

protected:
    const unsigned m_sample_rate; // The sample rate of the audio output device
    const unsigned m_channels; // The number of channels of the audio output device
    const unsigned m_frames_per_buffer; // The number of frames per buffer of the audio output device

    AudioBuffer * m_audio_buffer_link = nullptr; // A link to an audio buffer for storing audio data
};

#endif // AUDIO_OUTPUT_H