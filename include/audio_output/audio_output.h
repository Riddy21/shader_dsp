#pragma once
#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include <memory>
#include <chrono>

class AudioOutput {
public:
    /**
     * Constructor for the AudioOutputNew class.
     * 
     * @param frames_per_buffer The number of frames per buffer.
     * @param sample_rate The sample rate of the audio stream.
     * @param channels The number of channels in the audio stream.
     */
    AudioOutput(const unsigned frames_per_buffer, const unsigned sample_rate, const unsigned channels) :
        gid(generate_id()),
        m_frames_per_buffer(frames_per_buffer),
        m_sample_rate(sample_rate),
        m_channels(channels) {}

    /**
     * Destructor for the AudioOutputNew class.
     * 
     */
    virtual ~AudioOutput() {};

    const unsigned gid; // The unique identifier of the audio output device

    /**
     * Check if the audio output device is ready.
     * 
     * @return True if the audio output device is ready, false otherwise.
     */
    virtual bool is_ready() = 0;
    /**
     * Push audio data to the audio output device.
     * 
     * @param data The audio data to push.
     */
    virtual void push(const float * data) = 0;

    /**
     * Open the audio output device.
     * 
     * @return True if the audio output device was opened successfully, false otherwise.
     */
    virtual bool open() = 0;
    /**
     * Start the audio output device.
     * 
     * @return True if the audio output device was started successfully, false otherwise.
     */
    virtual bool start() = 0;
    /**
     * Stop the audio output device.
     * 
     * @return True if the audio output device was stopped successfully, false otherwise.
     */
    virtual bool stop() = 0;
    /**
     * Close the audio output device.
     * 
     * @return True if the audio output device was closed successfully, false otherwise.
     */
    virtual bool close() = 0;

protected:
    const unsigned m_frames_per_buffer; // The number of frames per buffer of the audio output device
    const unsigned m_sample_rate; // The sample rate of the audio output device
    const unsigned m_channels; // The number of channels of the audio output device

private:
    /**
     * Generate a unique identifier for the audio output device
     * 
     * @return The unique identifier for the audio output device.
     */
    static unsigned generate_id() {
        static unsigned id = 0;
        return id++;
    }
};

#endif // AUDIO_OUTPUT_H