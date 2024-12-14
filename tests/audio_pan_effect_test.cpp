#include "catch2/catch_all.hpp"
#include <thread>
#include <iostream>

#include "audio_output/audio_player_output.h"
#include "audio_core/audio_renderer.h"
#include "audio_core/audio_render_graph.h"
#include "audio_render_stage/audio_file_generator_render_stage.h"
#include "audio_render_stage/audio_gain_effect_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"

TEST_CASE("AudioGainEffectRenderStage") {
    auto audio_generator = new AudioFileGeneratorRenderStage(512, 44100, 2, "media/test.wav");
    auto effect_render_stage = new AudioGainEffectRenderStage(512, 44100, 2);
    auto final_render_stage = new AudioFinalRenderStage(512, 44100, 2);
    auto audio_driver = new AudioPlayerOutput(512, 44100, 2);

    audio_generator->find_parameter("output_audio_texture")->link(effect_render_stage->find_parameter("stream_audio_texture"));
    effect_render_stage->find_parameter("output_audio_texture")->link(final_render_stage->find_parameter("stream_audio_texture"));

    auto audio_render_graph = new AudioRenderGraph({audio_generator});

    AudioRenderer & audio_renderer = AudioRenderer::get_instance();

    auto play_param = audio_generator->find_parameter("gain");
    auto tone_param = audio_generator->find_parameter("tone");
    auto position_param = audio_generator->find_parameter("play_position");
    auto time_param = audio_renderer.find_global_parameter("global_time");

    auto balance_param = effect_render_stage->find_parameter("balance");

    // FIXME: Fix this example
    audio_renderer.add_render_graph(audio_render_graph);
    audio_renderer.add_render_output(audio_driver);

    // Open a thread to wait few sec and the shut it down
    std::thread t1([&audio_renderer, &audio_driver, &audio_generator, &play_param, &time_param, &position_param, &balance_param](){
        std::this_thread::sleep_for(std::chrono::seconds(1));
        position_param->set_value(time_param->get_value());
        play_param->set_value(1.0f);
        balance_param->set_value(0.0f);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        play_param->set_value(0.0f);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        position_param->set_value(time_param->get_value());
        play_param->set_value(1.0f);
        balance_param->set_value(1.0f);
        std::this_thread::sleep_for(std::chrono::seconds(5));
        play_param->set_value(0.0f);
        audio_renderer.terminate();
        //audio_driver.stop();
        //audio_driver.close();
    });

    REQUIRE(audio_renderer.initialize(512, 44100, 2));

    REQUIRE(audio_driver->open());
    REQUIRE(audio_driver->start());

    audio_renderer.start_main_loop();

    t1.detach();
}