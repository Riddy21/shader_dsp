#include "catch2/catch_all.hpp"

#include "audio_renderer.h"

TEST_CASE("AudioRenderer") {
    AudioRenderer audio_renderer(512, 2);
    audio_renderer.init();
    audio_renderer.terminate();
}