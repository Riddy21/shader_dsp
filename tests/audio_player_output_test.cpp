#include <catch2/catch_all.hpp>

#include <iostream>
#include <cmath>
#include <vector>
#include <mutex>

#include "audio_player_output.h"
#include "audio_buffer.h"

TEST_CASE("AudioPlayerOutputTest") {
    std::cout << "Hello, World!" << std::endl;

    int frames_per_buffer = 512;
    int channels = 2;

    // Generate the audio data
    float audio_data_interleaved[frames_per_buffer*channels];

    // Generate sine wave for both channels
    for (int i = 0; i < frames_per_buffer*channels; i=i+channels) {
        audio_data_interleaved[i] = sin(((double)(i)/((double)channels*(double)frames_per_buffer)) * M_PI * 10.0);
        audio_data_interleaved[i+1] = sin(((double)(i)/((double)channels*(double)frames_per_buffer)) * M_PI * 10.0);
    }

    // Create audio buffer
    AudioBuffer * audio_buffer = new AudioBuffer(1, frames_per_buffer*channels);
    audio_buffer->push(audio_data_interleaved, true);

    // Create the audio driver
    AudioPlayerOutput audio_driver(frames_per_buffer, 44100, channels);
    REQUIRE(audio_driver.set_buffer_link(audio_buffer));
    REQUIRE(audio_driver.open());
    REQUIRE(audio_driver.start());
    audio_driver.sleep(1);
    REQUIRE(audio_driver.stop());
    REQUIRE(audio_driver.close());
}