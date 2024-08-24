#include "catch2/catch_all.hpp"
#include <thread>
#include <iostream>
#include <mutex>

#include "audio_driver.h"
#include "audio_renderer.h"
#include "audio_generator_render_stage.h"

TEST_CASE("AudioGeneratorRenderStage") {
    AudioGeneratorRenderStage audio_generator(512, 44100, 2, "media/test.wav");
    AudioDriver audio_driver(512, 44100, 2);

    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    audio_renderer.add_render_stage(audio_generator);

    REQUIRE(audio_renderer.init(512, 44100, 2));

    // Open a thread to wait few sec and the shut it down
    std::thread t1([&audio_renderer, &audio_driver, &audio_generator](){
        std::this_thread::sleep_for(std::chrono::seconds(7));
        audio_driver.stop();
        audio_driver.close();
        audio_renderer.terminate();
    });

    REQUIRE(audio_driver.set_buffer_link(audio_renderer.get_output_buffer()));
    REQUIRE(audio_driver.open());
    REQUIRE(audio_driver.start());
    audio_renderer.main_loop();

    t1.detach();
}