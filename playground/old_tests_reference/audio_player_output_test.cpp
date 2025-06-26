#include <catch2/catch_all.hpp>
#include <cmath>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include "audio_output/audio_player_output.h"

const int SAMPLE_RATE = 44100;
const float INITIAL_AMPLITUDE = 0.5f;
const float INITIAL_FREQUENCY = 440.0f;
const int BUFFER_SIZE = 512;

std::atomic<float> amplitude(INITIAL_AMPLITUDE);
std::atomic<float> frequency(INITIAL_FREQUENCY);
std::atomic<bool> running(true);

int sampleIndex = 0;

void fillAudioBuffer(float *buffer, int bufferLength, float freq, float amp) {
    for (int i = 0; i < bufferLength; i += 2) {
        float sample = amp * std::sin(2.0f * M_PI * freq * sampleIndex / SAMPLE_RATE);
        buffer[i] = sample;
        buffer[i + 1] = sample;
        ++sampleIndex;
    }
}

void audioPlaybackLoop(AudioPlayerOutput& audioOutput) {
    float buffer[BUFFER_SIZE * 2];

    while (running) {
        if (audioOutput.is_ready()) {
            fillAudioBuffer(buffer, BUFFER_SIZE * 2, frequency.load(), amplitude.load());
            audioOutput.push(buffer);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
}

TEST_CASE("AudioSDLOutputNewTest") {
    AudioPlayerOutput audioOutput(BUFFER_SIZE, SAMPLE_RATE, 2);
    REQUIRE(audioOutput.open() == true);
    REQUIRE(audioOutput.start() == true);

    std::thread audioThread(audioPlaybackLoop, std::ref(audioOutput));

    std::this_thread::sleep_for(std::chrono::seconds(5));
    running = false;
    audioThread.join();

    REQUIRE(audioOutput.stop() == true);
    REQUIRE(audioOutput.close() == true);
}
