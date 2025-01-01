#include "catch2/catch_all.hpp"
#include <thread>

#include "audio_core/audio_renderer.h"
#include "audio_output/audio_player_output.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_core/audio_render_graph.h"
#include "keyboard/keyboard.h"

TEST_CASE("KeybaordTest") {
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    auto audio_driver = new AudioPlayerOutput(512, 44100, 2);

    Keyboard & keyboard = Keyboard::get_instance();

    auto final_render_stage = new AudioFinalRenderStage(512, 44100, 2);

    keyboard.get_output_render_stage()->connect_render_stage(final_render_stage);

    std::thread t1([&audio_renderer, &keyboard](){
        std::this_thread::sleep_for(std::chrono::seconds(1));
        Keyboard::key_down_callback('a', 0, 0);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        Keyboard::key_down_callback('s', 0, 0);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        Keyboard::key_up_callback('a', 0, 0);
        Keyboard::key_up_callback('s', 0, 0);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        audio_renderer.terminate();
    });

    auto audio_render_graph = new AudioRenderGraph(final_render_stage);

    audio_renderer.add_render_graph(audio_render_graph);

    audio_renderer.add_render_output(audio_driver);

    REQUIRE(audio_renderer.initialize(512, 44100, 2));
    REQUIRE(keyboard.initialize());


    REQUIRE(audio_driver->open());
    REQUIRE(audio_driver->start());

    audio_renderer.start_main_loop();

    t1.detach();

}
