#include <stdio.h>
#include <cstring>
#include <portaudio.h>

#include "audio_driver.h"

AudioDriver::AudioDriver(const unsigned frames_per_buffer, const unsigned sample_rate, const unsigned channels) : 
    stream(0),
    sample_rate(sample_rate),
    channels(channels),
    frames_per_buffer(frames_per_buffer)
{
    // Initialize the buffer links
    for (unsigned i = 0; i < channels; i++) {
        channel_buffer_links.push_back(new float[frames_per_buffer]);
    }
}

AudioDriver::~AudioDriver() {
    Pa_Terminate();
}

bool AudioDriver::open(PaDeviceIndex index) { PaStreamParameters outputParameters;
    PaError err;

    // Initialize PortAudio
    err = Pa_Initialize();
    if (err != paNoError) {
        error(err);
    }

    // Set up output stream with default device
    if (index == -1) {
        index = Pa_GetDefaultOutputDevice();
    }
    outputParameters.device = index;
    if (outputParameters.device == paNoDevice) {
        fprintf(stderr, "Error: No default output device.\n");
        return false;
    }

    // Set up output parameters
    outputParameters.channelCount = channels;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    // Set up the stream with the output parameters
    err = Pa_OpenStream(
        &stream,
        NULL,
        &outputParameters,
        sample_rate,
        frames_per_buffer,
        paClipOff,
        AudioDriver::audio_callback,
        this
    );
    if (err != paNoError) {
        error(err);
        return false;
    }
    printf("Opened stream with device: %s\n", Pa_GetDeviceInfo(outputParameters.device)->name);
    printf("Sample rate: %d\n", sample_rate);
    printf("Frames per buffer: %d\n", frames_per_buffer);
    printf("Channels: %d\n", channels);
    printf("Latency: %f\n", outputParameters.suggestedLatency);
    return true;
}

bool AudioDriver::start() {
    if (stream == 0) {
        fprintf(stderr, "Error: Stream not open.\n");
        return false;
    }
    // Check that all the channels have been linked
    for (unsigned i = 0; i < channels; i++) {
        if (channel_buffer_links[i] == nullptr) {
            fprintf(stderr, "Error: Channel %d not linked.\n", i+1);
            return false;
        }
    }
    // Start the stream
    PaError err = Pa_StartStream(stream);
    if (err != paNoError) {
        error(err);
        return false;
    }
    printf("Started stream.\n");
    return true;
}

bool AudioDriver::stop() {
    if (stream == 0) {
        fprintf(stderr, "Error: Stream not open.\n");
        return false;
    }
    // Stop the stream
    PaError err = Pa_StopStream(stream);
    if (err != paNoError) {
        error(err);
        return false;
    }
    printf("Stopped stream.\n");
    return true;
}

bool AudioDriver::close() {
    if (stream == 0) {
        fprintf(stderr, "Error: Stream not open.\n");
        return false;
    }
    // Close the stream
    PaError err = Pa_CloseStream(stream);
    if (err != paNoError) {
        error(err);
        return false;
    }
    printf("Closed stream.\n");
    return true;
}

void AudioDriver::sleep(const unsigned seconds) {
    printf("Sleeping for %d seconds.\n", seconds);
    Pa_Sleep(1000*seconds);
}

void AudioDriver::error(PaError err) {
    fprintf(stderr, "portaudio error: %s\n", Pa_GetErrorText(err));
    fprintf(stderr, "error number: %d\n", err);
    fprintf(stderr, "error message: %s\n", Pa_GetErrorText(err));
    Pa_Terminate();
}

bool AudioDriver::set_buffer_link(const std::vector<float> & buffer_vector, const unsigned channel) {
    if (channel <= 0 || channel > channels) {
        fprintf(stderr, "Error: Channel %d out of range.\n", channel);
        return false;
    }
    if (buffer_vector.size() != frames_per_buffer) {
        fprintf(stderr, "Error: Buffer size does not match frames per buffer.\n");
        return false;
    }
    channel_buffer_links[channel - 1] = buffer_vector.data();
    printf("Linked buffer to channel %d.\n", channel);
    return true;
}

int AudioDriver::audio_callback(const void *input_buffer, void *output_buffer,
                                unsigned long frames_per_buffer, const PaStreamCallbackTimeInfo *time_info,
                                PaStreamCallbackFlags status_flags, void *user_data) {
    AudioDriver * driver = (AudioDriver *) user_data;
    float * out = (float *) output_buffer;
    // TODO: Think about how to make this faster by doing the interleaving earlier
    // interleave the channels
    for (unsigned i = 0; i < frames_per_buffer; i++) {
        for (unsigned j = 0; j < driver->channels; j++) {
            *out++ = driver->channel_buffer_links[j][i];
        }
    }
    return paContinue;
}