#include "catch2/catch_all.hpp"

#include "audio_file_output.h"

TEST_CASE("AudioFileOutputTest") {
    AudioFileOutput audio_file_output("media/output.wav", 512, 44100, 2);
    audio_file_output.open();
    audio_file_output.start();
    audio_file_output.stop();
    audio_file_output.close();
}
