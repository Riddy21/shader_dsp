#include <stdio.h>
#include <cstring>
#include <memory>
#include <portaudio.h>

#include "audio_player_output.h"


AudioPlayerOutput::~AudioPlayerOutput() {
    Pa_Terminate();
}

bool AudioPlayerOutput::open(PaDeviceIndex index) { PaStreamParameters outputParameters;
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

    //// Get all the audio devices
    //int numDevices = Pa_GetDeviceCount();
    //for (int i = 0; i < numDevices; i++) {
    //    const PaDeviceInfo *deviceInfo;
    //    deviceInfo = Pa_GetDeviceInfo(i);
    //    printf("Device %d: %s\n", i, deviceInfo->name);
    //    printf("Audio API: %s\n", Pa_GetHostApiInfo(deviceInfo->hostApi)->name);
    //}

    outputParameters.device = index;
    if (outputParameters.device == paNoDevice) {
        fprintf(stderr, "Error: No default output device.\n");
        return false;
    }

    // Set up output parameters
    outputParameters.channelCount = m_channels;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = 0.0;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    // Set up the stream with the output parameters
    err = Pa_OpenStream(
        &m_stream,
        NULL,
        &outputParameters,
        m_sample_rate,
        m_frames_per_buffer,
        paClipOff,
        AudioPlayerOutput::audio_callback,
        this
    );
    if (err != paNoError) {
        error(err);
        return false;
    }
    printf("Opened stream with device: %s\n", Pa_GetDeviceInfo(outputParameters.device)->name);
    printf("Sample rate: %d\n", m_sample_rate);
    printf("Frames per buffer: %d\n", m_frames_per_buffer);
    printf("Channels: %d\n", m_channels);
    printf("Latency: %f\n", outputParameters.suggestedLatency);
    return true;
}

bool AudioPlayerOutput::start() {
    if (m_stream == 0) {
        fprintf(stderr, "Error: Stream not open.\n");
        return false;
    }
    // Check that all the channels have been linked
    if (m_audio_buffer_link == nullptr) {
        fprintf(stderr, "Error: Buffer not linked.\n");
        return false;
    }
    // Start the stream
    PaError err = Pa_StartStream(m_stream);
    if (err != paNoError) {
        error(err);
        return false;
    }
    printf("Started stream.\n");
    return true;
}

bool AudioPlayerOutput::stop() {
    if (m_stream == 0) {
        fprintf(stderr, "Error: Stream not open.\n");
        return false;
    }
    // Stop the stream
    PaError err = Pa_StopStream(m_stream);
    if (err != paNoError) {
        error(err);
        return false;
    }
    printf("Stopped stream.\n");
    return true;
}

bool AudioPlayerOutput::close() {
    if (m_stream == 0) {
        fprintf(stderr, "Error: Stream not open.\n");
        return false;
    }
    // Close the stream
    PaError err = Pa_CloseStream(m_stream);
    if (err != paNoError) {
        error(err);
        return false;
    }
    printf("Closed stream.\n");
    return true;
}

void AudioPlayerOutput::sleep(const unsigned seconds) {
    printf("Sleeping for %d seconds.\n", seconds);
    Pa_Sleep(1000*seconds);
}

void AudioPlayerOutput::error(PaError err) {
    fprintf(stderr, "portaudio error: %s\n", Pa_GetErrorText(err));
    fprintf(stderr, "error number: %d\n", err);
    fprintf(stderr, "error message: %s\n", Pa_GetErrorText(err));
    Pa_Terminate();
}

int AudioPlayerOutput::audio_callback(const void *input_buffer, void *output_buffer,
                                unsigned long frames_per_buffer, const PaStreamCallbackTimeInfo *time_info,
                                PaStreamCallbackFlags status_flags, void *user_data) {
    AudioPlayerOutput * driver = (AudioPlayerOutput *) user_data;
    float * out = (float *) output_buffer;

    // FIXME: Notify buffer driver that we are ready for the next buffer
    const float * buffer = driver->m_audio_buffer_link->pop();
    // Copy the audio buffer to the output buffer
    memcpy(out, buffer, frames_per_buffer * driver->m_channels * sizeof(float));

    return paContinue;
}