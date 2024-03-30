#include <stdio.h>
#include <portaudio.h>

#include "audio_driver.h"

AudioDriver::AudioDriver(const int sample_rate, const int frames_per_buffer, const int channels) : 
    stream(0),
    sample_rate(sample_rate),
    channels(channels),
    frames_per_buffer(frames_per_buffer)
{};

bool AudioDriver::open(PaDeviceIndex index) { PaStreamParameters outputParameters;
    PaError err;

    // Initialize PortAudio
    err = Pa_Initialize();
    if (err != paNoError) {
        error(err);
    }

    // Set up output stream with default device
    // TODO: Consider choosing device in the future
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
        NULL,
        NULL
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

void AudioDriver::sleep(const int seconds) {
    printf("Sleeping for %d seconds.\n", seconds);
    Pa_Sleep(1000*seconds);
}

void AudioDriver::error(PaError err) {
    fprintf(stderr, "portaudio error: %s\n", Pa_GetErrorText(err));
    fprintf(stderr, "error number: %d\n", err);
    fprintf(stderr, "error message: %s\n", Pa_GetErrorText(err));
    Pa_Terminate();
}