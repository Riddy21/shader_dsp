#include <catch2/catch_all.hpp>

#include <iostream>
#include <cmath>
#include <vector>

#include "audio_driver.h"

TEST_CASE("AudioDriver") {
    std::cout << "Hello, World!" << std::endl;

    int frames_per_buffer = 512;

    // Generate the audio data
    std::vector<float> audio_data_left(frames_per_buffer);
    std::vector<float> audio_data_right(frames_per_buffer);

    // Generate sine wave for both channels
    for (int i = 0; i < frames_per_buffer; i++) {
        audio_data_left[i] = sin(((double)i/(double)frames_per_buffer) * M_PI * 10.0);
        audio_data_right[i] = sin(((double)i/(double)frames_per_buffer) * M_PI * 20.0);
    }

    // Create the audio driver
    AudioDriver audio_driver(44100, frames_per_buffer, 2);
    REQUIRE(audio_driver.set_buffer_link(audio_data_left, 1));
    REQUIRE(audio_driver.set_buffer_link(audio_data_right, 2));
    REQUIRE(audio_driver.open());
    REQUIRE(audio_driver.start());
    audio_driver.sleep(1);
    REQUIRE(audio_driver.stop());
    REQUIRE(audio_driver.close());
}