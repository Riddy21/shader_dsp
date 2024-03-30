#pragma once
#include <portaudio.h>

class AudioDriver {
public:
    AudioDriver(const int sample_rate, const int frames_per_buffer, const int channels);
    bool open(PaDeviceIndex index=-1);
    bool start();
    void sleep(const int seconds);
    bool stop();
    bool close();

private:
    static int audio_callback(const void *input_buffer, void *output_buffer,
                              unsigned long frames_per_buffer, const PaStreamCallbackTimeInfo *time_info,
                              PaStreamCallbackFlags status_flags, void *user_data);
    void error(PaError err);

    PaStream *stream;
    const int sample_rate;
    const int channels;
    const int frames_per_buffer;
};