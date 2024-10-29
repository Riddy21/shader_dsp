#include <iostream>
#include <cmath>
#include <thread>
#include <portaudio.h>

const double PI = 3.14159265358979323846;
const int SAMPLE_RATE = 44100;
const int FRAMES_PER_BUFFER = 256;
const int NUM_CHANNELS = 2;
const double FREQUENCY = 440.0; // Frequency of the sine wave (A4)

void generate_sine_wave(float* buffer, int frames, int channels, double frequency, double sample_rate, double& phase) {
    double phase_increment = 2.0 * PI * frequency / sample_rate;
    for (int i = 0; i < frames; ++i) {
        float sample = static_cast<float>(sin(phase));
        for (int j = 0; j < channels; ++j) {
            buffer[i * channels + j] = sample;
        }
        phase += phase_increment;
        if (phase >= 2.0 * PI) {
            phase -= 2.0 * PI;
        }
    }
}

int main() {
    PaError err;
    PaStream* stream;
    PaStreamParameters outputParameters;
    double phase = 0.0;

    // Initialize PortAudio
    err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "Failed to initialize PortAudio: " << Pa_GetErrorText(err) << std::endl;
        return 1;
    }

    // Set up output parameters
    outputParameters.device = Pa_GetDefaultOutputDevice();
    if (outputParameters.device == paNoDevice) {
        std::cerr << "Error: No default output device." << std::endl;
        return 1;
    }
    outputParameters.channelCount = NUM_CHANNELS;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    // Open the stream
    err = Pa_OpenStream(
        &stream,
        NULL,
        &outputParameters,
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        paClipOff,
        NULL,
        NULL
    );
    if (err != paNoError) {
        std::cerr << "Failed to open PortAudio stream: " << Pa_GetErrorText(err) << std::endl;
        return 1;
    }

    // Start the stream
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        std::cerr << "Failed to start PortAudio stream: " << Pa_GetErrorText(err) << std::endl;
        return 1;
    }

    // Generate and write audio data
    float buffer[FRAMES_PER_BUFFER * NUM_CHANNELS];
    int counter = 0;
    while (true) {
        generate_sine_wave(buffer, FRAMES_PER_BUFFER, NUM_CHANNELS, FREQUENCY, SAMPLE_RATE, phase);
        err = Pa_WriteStream(stream, buffer, FRAMES_PER_BUFFER);
        if (err != paNoError) {
            if (err == paOutputUnderflowed) {
                std::cerr << "Output underflowed." << std::endl;
            } else {
                std::cerr << "Failed to write to PortAudio stream: " << Pa_GetErrorText(err) << std::endl;
                break;
            }
        }
        printf("Wrote frame %d\n", counter++);
        //static auto next_frame_time = std::chrono::steady_clock::now();
        //next_frame_time += std::chrono::microseconds(1000000 * FRAMES_PER_BUFFER / SAMPLE_RATE);
        //std::this_thread::sleep_until(next_frame_time);
    }

    // Stop the stream
    err = Pa_StopStream(stream);
    if (err != paNoError) {
        std::cerr << "Failed to stop PortAudio stream: " << Pa_GetErrorText(err) << std::endl;
    }

    // Close the stream
    err = Pa_CloseStream(stream);
    if (err != paNoError) {
        std::cerr << "Failed to close PortAudio stream: " << Pa_GetErrorText(err) << std::endl;
    }

    // Terminate PortAudio
    Pa_Terminate();

    return 0;
}