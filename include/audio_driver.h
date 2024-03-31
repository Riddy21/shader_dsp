#pragma once
#ifndef AUDIO_DRIVER_H
#define AUDIO_DRIVER_H

#include <vector>

#include <portaudio.h>

class AudioDriver {
public:
    /**
     * Constructor for the AudioDriver class.
     * 
     * @param sample_rate The sample rate of the audio stream.
     * @param frames_per_buffer The number of frames per buffer.
     * @param channels The number of channels in the audio stream.
    */
    AudioDriver(const unsigned sample_rate, const unsigned frames_per_buffer, const unsigned channels);
    /**
     * Destructor for the AudioDriver class.
    */
    ~AudioDriver();
    /**
     * Link a buffer to the audio driver
     * 
     * @param buffer_pointer The pointer to the buffer to link to the audio driver.
     * @return True if the buffer was linked successfully, false otherwise.
    */
    bool set_buffer_link(const std::vector<float> & buffer_pointer, const unsigned channel);
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

private:
    static int audio_callback(const void *input_buffer, void *output_buffer,
                              unsigned long frames_per_buffer, const PaStreamCallbackTimeInfo *time_info,
                              PaStreamCallbackFlags status_flags, void *user_data);
    static void error(PaError err);

    PaStream *stream;
    std::vector<const float *> channel_buffer_links;
    const unsigned sample_rate;
    const unsigned channels;
    const unsigned frames_per_buffer;
};

#endif