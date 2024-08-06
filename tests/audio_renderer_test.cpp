#include "catch2/catch_all.hpp"

#include "audio_renderer.h"

TEST_CASE("AudioRenderer") {
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    REQUIRE(audio_renderer.init(512, 2));
    
    // TODO: Add a render stage and see if it works

    audio_renderer.main_loop();
    REQUIRE(audio_renderer.terminate());
    REQUIRE(audio_renderer.cleanup());
}