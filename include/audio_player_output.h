#pragma once
#ifndef AUDIO_DRIVER_H
#define AUDIO_DRIVER_H

#include <vector>
#include <mutex>
#include <portaudio.h>
#include <memory>

#include "audio_buffer.h"
#include "audio_output.h"

class AudioPlayerOutput : public AudioOutput {
public:
    /**
     * Constructor for the AudioDriver class.
     * 
     * @param sample_rate The sample rate of the audio stream.
     * @param frames_per_buffer The number of frames per buffer.
     * @param channels The number of channels in the audio stream.
    */
    AudioPlayerOutput(const unsigned frames_per_buffer, const unsigned sample_rate, const unsigned channels) : 
        AudioOutput(frames_per_buffer, sample_rate, channels),
        m_stream(0) {}
    /**
     * Destructor for the AudioDriver class.
    */
    ~AudioPlayerOutput();
    /**
     * Open the audio stream.
     * 
     * @param index The index of the audio device to open, -1 for default device.
     * @return True if the audio stream was opened successfully, false otherwise.
    */
    bool open(PaDeviceIndex index=-1);
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
    void write_audio(const float * buffer);

private:
    static int audio_callback(const void *input_buffer, void *output_buffer,
                              unsigned long frames_per_buffer, const PaStreamCallbackTimeInfo *time_info,
                              PaStreamCallbackFlags status_flags, void *user_data);
    static void write_audio_callback(AudioPlayerOutput* audio_player_output);
    static void error(PaError err);

    PaStream *m_stream;
    bool m_is_running = false;
};

#endif