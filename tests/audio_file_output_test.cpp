#include "catch2/catch_all.hpp"
#include <fstream>
#include <thread>
#include <iostream>

#include "audio_buffer.h"
#include "audio_file_output.h"
#include "audio_wav.h"

TEST_CASE("AudioFileOutputTest") {

    int frames_per_buffer = 512;
    int channels = 2;

    // Generate the audio data
    float audio_data_interleaved[frames_per_buffer*channels];

    // Generate sine wave for both channels
    for (int i = 0; i < frames_per_buffer*channels; i=i+channels) {
        audio_data_interleaved[i] = sin(((double)(i)/((double)channels*(double)frames_per_buffer)) * M_PI * 10.0);
        audio_data_interleaved[i+1] = sin(((double)(i)/((double)channels*(double)frames_per_buffer)) * M_PI * 10.0);
    }

    AudioFileOutput audio_file_output(512, 44100, 2, "build/tests/output.wav");
    REQUIRE(audio_file_output.open());
    REQUIRE(audio_file_output.start());
    for (int i = 0; i < 100; i++) {
        audio_file_output.push(audio_data_interleaved);
    }
    REQUIRE(audio_file_output.stop());
    REQUIRE(audio_file_output.close());

    // Just for debugging, read the file and print the values
    std::ifstream file ("build/tests/output.wav", std::ios::binary);

    WAVHeader header;
    file.read((char *) &header, sizeof(WAVHeader));

    if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0) {
        std::cerr << "Invalid audio file format: " << std::endl;
        exit(1);
    }

    if (header.format_type != 1) {
        std::cerr << "Invalid audio file format type: " << std::endl;
        exit(1);
    }

    // print info 
    std::cout << "Channels: " << header.channels << std::endl;
    std::cout << "Sample rate: " << header.sample_rate << std::endl;
    std::cout << "Bits per sample: " << header.bits_per_sample << std::endl;
    std::cout << "Data size: " << header.data_size << std::endl;
}
