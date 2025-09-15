#pragma once
#ifndef AUDIO_SDL_PLAYER_OUTPUT_H
#define AUDIO_SDL_PLAYER_OUTPUT_H

#include <vector>
#include <mutex>
#include <SDL2/SDL.h>
#include <memory>

#include "audio_output/audio_output.h"

class AudioPlayerOutput : public AudioOutput {
public:
    /**
     * Constructor for the AudioSDLOutputNew class.
     * 
     * @param frames_per_buffer The number of frames per buffer.
     * @param sample_rate The sample rate of the audio stream.
     * @param channels The number of channels in the audio stream.
    */
    AudioPlayerOutput(const unsigned frames_per_buffer, const unsigned sample_rate, const unsigned channels) : 
        AudioOutput(frames_per_buffer, sample_rate, channels),
        m_device_id(0) {}
    /**
     * Destructor for the AudioSDLOutputNew class.
    */
    ~AudioPlayerOutput() override;
    /**
     * Check if the audio output device is ready.
     * 
     * @return True if the audio output device is ready, false otherwise.
    */
    bool is_ready() override;
    /**
     * Return the number of bytes currently queued in SDL for playback.
     * This can be used in tests to confirm that the device is consuming data.
     */
    size_t queued_bytes() const;
    /**
     * Clear the audio queue, removing all pending audio data.
     * This is useful for test cleanup to ensure tests start with a clean state.
     */
    void clear_queue();
    /**
     * Push audio data to the audio output device.
     * 
     * @param data The audio data to push.
    */
    void push(const float * data) override;
    // TODO: Add device selection and error handling
    /**
     * Open the audio output device.
     * 
     * @return True if the audio output device was opened successfully, false otherwise.
    */
    bool open() override;
    /**
     * Start the audio output device.
     * 
     * @return True if the audio output device was started successfully, false otherwise.
    */
    bool start() override;
    /**
     * Stop the audio output device.
     * 
     * @return True if the audio output device was stopped successfully, false otherwise.
    */
    bool stop() override;
    /**
     * Close the audio output device.
     * 
     * @return True if the audio output device was closed successfully, false otherwise.
    */
    bool close() override;

private:
    /**
     * Error handling function.
     */
    static void error(const char* message);

    SDL_AudioDeviceID m_device_id;
    bool m_is_running = false;
};

#endif
