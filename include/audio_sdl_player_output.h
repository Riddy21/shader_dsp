#pragma once
#ifndef AUDIO_SDL_PLAYER_OUTPUT_H
#define AUDIO_SDL_PLAYER_OUTPUT_H

#include <vector>
#include <mutex>
#include <SDL2/SDL.h>
#include <memory>

#include "audio_buffer.h"
#include "audio_output.h"

class AudioSDLPlayerOutput : public AudioOutput {
public:
    /**
     * Constructor for the AudioSDLPlayerOutput class.
     * 
     * @param sample_rate The sample rate of the audio stream.
     * @param frames_per_buffer The number of frames per buffer.
     * @param channels The number of channels in the audio stream.
    */
    AudioSDLPlayerOutput(const unsigned frames_per_buffer, const unsigned sample_rate, const unsigned channels) : 
        AudioOutput(frames_per_buffer, sample_rate, channels),
        m_device_id(0) {}
    /**
     * Destructor for the AudioSDLPlayerOutput class.
    */
    ~AudioSDLPlayerOutput();
    /**
     * Open the audio stream.
     * 
     * @param device_name The name of the audio device to open, nullptr for default device.
     * @return True if the audio stream was opened successfully, false otherwise.
    */
    bool open(const char* device_name = nullptr);
    /**
     * Start the audio stream.
     * 
     * @return True if the audio stream was started successfully, false otherwise.
    */
    bool start();
    /**
     * Sleep for a given number of seconds.
     * 
     * @param seconds The number of seconds to sleep.
    */
    void sleep(const unsigned seconds);
    /**
     * Stop the audio stream.
     * 
     * @return True if the audio stream was stopped successfully, false otherwise.
    */
    bool stop();
    /**
     * Close the audio stream.
     * 
     * @return True if the audio stream was closed successfully, false otherwise.
    */
    bool close();

private:
    static void audio_callback(void* userdata, Uint8* stream, int len);

    static void error(const char* message);

    SDL_AudioDeviceID m_device_id;
    bool m_is_running = false;
};

#endif
