#include "catch2/catch_all.hpp"
#include <thread>

#include "audio_renderer.h"
#include "audio_generator_render_stage.h"

TEST_CASE("AudioGeneratorRenderStage") {
    AudioGeneratorRenderStage audio_generator(512, 44100, 2, "media/test.wav");

    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    audio_renderer.add_render_stage(audio_generator);

    REQUIRE(audio_renderer.init(512, 2));

    // Open a thread to wait 1 sec and the shut it down
    std::thread t1([&audio_renderer](){
        // Wait for 1 sec
        std::this_thread::sleep_for(std::chrono::seconds(5));
        audio_renderer.terminate();
    });

    t1.detach();

    audio_renderer.main_loop();
}