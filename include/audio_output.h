#pragma once
#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include <memory>
#include <chrono>

#include "audio_buffer.h"

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
        gid(generate_id()),
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
     * Get the latency of the audio output device
     * 
     * @param latency The latency of the audio output device in microseconds.
     */
    int get_latency() {
        return m_latency;
    }

    /**
     * Open the output device
     * 
     * @return True if the output device was opened successfully, false otherwise.
     */
    virtual bool open() = 0;

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

    /**
     * Close the output device
     * 
     * @return True if the output device was closed successfully, false otherwise.
     */
    virtual bool close() = 0;

    const unsigned int gid;

protected:
    const unsigned m_sample_rate; // The sample rate of the audio output device
    const unsigned m_channels; // The number of channels of the audio output device
    const unsigned m_frames_per_buffer; // The number of frames per buffer of the audio output device
    unsigned m_latency = 0; // The average latency of the audio output device in microseconds

    AudioBuffer * m_audio_buffer_link = nullptr; // A link to an audio buffer for storing audio data

    /**
     * Generate a unique identifier for the audio output device
     * 
     * @return The unique identifier for the audio output device.
     */
    static unsigned generate_id() {
        static unsigned id = 0;
        return id++;
    }

    void update_latency();
};

#endif // AUDIO_OUTPUT_H