#include "catch2/catch_all.hpp"
#include <thread>
#include <iostream>
#include <stdexcept>

#include "audio_output/audio_player_output.h"
#include "audio_core/audio_renderer.h"
#include "audio_render_stage/audio_file_generator_render_stage.h"
#include "audio_render_stage/audio_gain_effect_render_stage.h"
#include "audio_render_stage/audio_multitrack_join_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_core/audio_render_graph.h"


TEST_CASE("AudioGainEffectRenderStage") {
    // Generate a render stage graph

    // TODO: Add name to each render stage
    auto audio_generator = new AudioFileGeneratorRenderStage(512, 44100, 2, "media/test.wav");
    auto effect_render_stage = new AudioGainEffectRenderStage(512, 44100, 2);

    auto audio_generator_2 = new AudioFileGeneratorRenderStage(512, 44100, 2, "media/test.wav");
    auto effect_render_stage_2 = new AudioGainEffectRenderStage(512, 44100, 2);

    auto join_render_stage = new AudioMultitrackJoinRenderStage(512, 44100, 2, 2);
    auto final_render_stage = new AudioFinalRenderStage(512, 44100, 2);

    // Get the parameters from the audio_generator render stage
    AudioRenderGraph::link_render_stages(audio_generator, effect_render_stage);
    AudioRenderGraph::link_render_stages(audio_generator_2, effect_render_stage_2);

    AudioRenderGraph::link_render_stages(effect_render_stage, join_render_stage);
    AudioRenderGraph::link_render_stages(effect_render_stage_2, join_render_stage);

    AudioRenderGraph::link_render_stages(join_render_stage, final_render_stage);

    // TODO: Convert this into render graph object
    auto audio_render_graph = new AudioRenderGraph({audio_generator, audio_generator_2});

    REQUIRE(audio_render_graph != nullptr);

    // set up the audio renderer
    auto audio_driver = new AudioPlayerOutput(512, 44100, 2);

    AudioRenderer & audio_renderer = AudioRenderer::get_instance();

    audio_renderer.add_render_graph(audio_render_graph);

    audio_renderer.add_render_output(audio_driver);

    // Open a thread to wait few sec and the shut it down
    std::thread t1([&audio_renderer, &audio_driver, &audio_generator, &audio_generator_2, &effect_render_stage, &effect_render_stage_2](){
        auto time_param = audio_renderer.find_global_parameter("global_time");

        auto position_param = audio_generator->find_parameter("play_position");
        auto play_param = audio_generator->find_parameter("gain");
        auto balance_param = effect_render_stage->find_parameter("balance");

        auto position_param_2 = audio_generator_2->find_parameter("play_position");
        auto play_param_2 = audio_generator_2->find_parameter("gain");
        auto balance_param_2 = effect_render_stage_2->find_parameter("balance");

        play_param->set_value(0.0f);
        play_param_2->set_value(0.0f);

        std::this_thread::sleep_for(std::chrono::seconds(1));
        // Play from left speaker on track 1
        position_param->set_value(time_param->get_value());
        balance_param->set_value(0.0f);
        play_param->set_value(1.0f);

        std::this_thread::sleep_for(std::chrono::seconds(1));
        // Play from right speaker on track 2
        position_param_2->set_value(time_param->get_value());
        balance_param_2->set_value(1.0f);
        play_param_2->set_value(1.0f);

        std::this_thread::sleep_for(std::chrono::seconds(1));
        // Stop track 1
        play_param->set_value(0.0f);

        std::this_thread::sleep_for(std::chrono::seconds(2));

        audio_renderer.terminate();
    });

    REQUIRE(audio_renderer.initialize(512, 44100, 2));

    REQUIRE(audio_driver->open());
    REQUIRE(audio_driver->start());

    audio_renderer.start_main_loop();

    t1.detach();
}

TEST_CASE("AudioGainEffectRenderStage_bad") {
    // Generate a render stage graph

    auto audio_generator = new AudioFileGeneratorRenderStage(512, 44100, 2, "media/test.wav");
    auto effect_render_stage = new AudioGainEffectRenderStage(512, 44100, 2);

    auto audio_generator_2 = new AudioFileGeneratorRenderStage(512, 44100, 2, "media/test.wav");
    auto effect_render_stage_2 = new AudioGainEffectRenderStage(512, 44100, 2);

    auto join_render_stage = new AudioMultitrackJoinRenderStage(512, 44100, 2, 2);
    auto final_render_stage = new AudioFinalRenderStage(512, 44100, 2);

    // Get the parameters from the audio_generator render stage
    AudioRenderGraph::link_render_stages(audio_generator, effect_render_stage);
    AudioRenderGraph::link_render_stages(audio_generator_2, effect_render_stage_2);

    REQUIRE(!AudioRenderGraph::link_render_stages(audio_generator, effect_render_stage_2));

    AudioRenderGraph::link_render_stages(effect_render_stage, join_render_stage);
    AudioRenderGraph::link_render_stages(effect_render_stage_2, join_render_stage);

    REQUIRE(AudioRenderGraph::unlink_render_stages(audio_generator, effect_render_stage));
    REQUIRE(AudioRenderGraph::unlink_render_stages(effect_render_stage_2, join_render_stage));

    AudioRenderGraph::link_render_stages(join_render_stage, final_render_stage);

    // TODO: Convert this into render graph object
    
    REQUIRE_THROWS_AS(AudioRenderGraph({audio_generator, audio_generator_2}), std::runtime_error);
}


