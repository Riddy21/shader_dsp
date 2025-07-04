#include "catch2/catch_all.hpp"
#include <thread>
#include <iostream>
#include <stdexcept>

#include "audio_output/audio_player_output.h"
#include "audio_core/audio_renderer.h"
#include "audio_render_stage/audio_file_generator_render_stage.h"
#include "audio_render_stage/audio_effect_render_stage.h"
#include "audio_render_stage/audio_multitrack_join_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_core/audio_render_graph.h"
#include "engine/event_loop.h"

TEST_CASE("AudioRenderGraph_test") {
    // Generate a render stage graph
    auto audio_generator = new AudioFileGeneratorRenderStage(512, 44100, 2, "media/test.wav");
    auto effect_render_stage = new AudioGainEffectRenderStage(512, 44100, 2);
    auto audio_generator_2 = new AudioFileGeneratorRenderStage(512, 44100, 2, "media/test.wav");
    auto effect_render_stage_2 = new AudioGainEffectRenderStage(512, 44100, 2);
    auto join_render_stage = new AudioMultitrackJoinRenderStage(512, 44100, 2, 2);
    auto final_render_stage = new AudioFinalRenderStage(512, 44100, 2);

    // print the gids
    std::cout << "AudioGenerator GID: " << audio_generator->gid << std::endl;
    std::cout << "AudioGenerator 2 GID: " << audio_generator_2->gid << std::endl;
    std::cout << "EffectRenderStage GID: " << effect_render_stage->gid << std::endl;
    std::cout << "EffectRenderStage 2 GID: " << effect_render_stage_2->gid << std::endl;
    std::cout << "JoinRenderStage GID: " << join_render_stage->gid << std::endl;
    std::cout << "FinalRenderStage GID: " << final_render_stage->gid << std::endl;
    
    // Connect render stages
    audio_generator->connect_render_stage(effect_render_stage);
    audio_generator_2->connect_render_stage(effect_render_stage_2);
    effect_render_stage->connect_render_stage(join_render_stage);
    effect_render_stage_2->connect_render_stage(join_render_stage);
    join_render_stage->connect_render_stage(final_render_stage);

    auto audio_render_graph = new AudioRenderGraph(final_render_stage);
    REQUIRE(audio_render_graph != nullptr);

    // Set up the audio renderer
    auto audio_driver = new AudioPlayerOutput(512, 44100, 2);
    AudioRenderer& audio_renderer = AudioRenderer::get_instance();
    audio_renderer.add_render_graph(audio_render_graph);
    audio_renderer.add_render_output(audio_driver);

    EventLoop& event_loop = EventLoop::get_instance();

    // Open a thread to control playback and terminate the event loop
    std::thread t1([&audio_renderer, &audio_generator, &audio_generator_2, &effect_render_stage, &effect_render_stage_2, &event_loop]() {
        auto time_param = audio_renderer.find_global_parameter("global_time");

        auto balance_param = effect_render_stage->find_parameter("balance");

        auto balance_param_2 = effect_render_stage_2->find_parameter("balance");

        std::this_thread::sleep_for(std::chrono::seconds(1));
        // Play from left speaker on track 1
        audio_generator->play_note(261.63f, 1.0f);
        balance_param->set_value(0.0f);

        std::this_thread::sleep_for(std::chrono::seconds(1));
        // Play from right speaker on track 2
        balance_param_2->set_value(1.0f);
        audio_generator_2->play_note(261.63f, 1.0f);

        std::this_thread::sleep_for(std::chrono::seconds(2));
        // Stop track 1
        audio_generator->stop_note(261.63f);

        std::this_thread::sleep_for(std::chrono::seconds(2));

        audio_generator_2->stop_note(261.63f);

        event_loop.terminate();
    });

    REQUIRE(audio_renderer.initialize(512, 44100, 2));
    REQUIRE(audio_driver->open());
    REQUIRE(audio_driver->start());

    event_loop.add_loop_item(&audio_renderer);
    event_loop.run_loop();

    t1.join();
}

TEST_CASE("AudioRenderGraph_inputs") {
    // Generate a render stage graph
    auto audio_generator = new AudioSingleShaderFileGeneratorRenderStage(512, 44100, 2, "media/test.wav");
    auto effect_render_stage = new AudioGainEffectRenderStage(512, 44100, 2);
    auto audio_generator_2 = new AudioSingleShaderFileGeneratorRenderStage(512, 44100, 2, "media/test.wav");
    auto effect_render_stage_2 = new AudioGainEffectRenderStage(512, 44100, 2);
    auto join_render_stage = new AudioMultitrackJoinRenderStage(512, 44100, 2, 2);
    auto final_render_stage = new AudioFinalRenderStage(512, 44100, 2);

    // Connect render stages
    audio_generator->connect_render_stage(effect_render_stage);
    audio_generator_2->connect_render_stage(effect_render_stage_2);
    effect_render_stage->connect_render_stage(join_render_stage);
    effect_render_stage_2->connect_render_stage(join_render_stage);
    join_render_stage->connect_render_stage(final_render_stage);

    auto audio_render_graph = new AudioRenderGraph({audio_generator, audio_generator_2});
    REQUIRE(audio_render_graph != nullptr);

    // Set up the audio renderer
    auto audio_driver = new AudioPlayerOutput(512, 44100, 2);
    AudioRenderer& audio_renderer = AudioRenderer::get_instance();
    audio_renderer.add_render_graph(audio_render_graph);
    audio_renderer.add_render_output(audio_driver);

    EventLoop& event_loop = EventLoop::get_instance();

    // Open a thread to control playback and terminate the event loop
    std::thread t1([&audio_renderer, &audio_generator, &audio_generator_2, &effect_render_stage, &effect_render_stage_2, &event_loop]() {
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

        std::this_thread::sleep_for(std::chrono::seconds(2));
        // Stop track 1
        play_param->set_value(0.0f);

        std::this_thread::sleep_for(std::chrono::seconds(2));

        event_loop.terminate();
    });

    REQUIRE(audio_renderer.initialize(512, 44100, 2));
    REQUIRE(audio_driver->open());
    REQUIRE(audio_driver->start());

    event_loop.add_loop_item(&audio_renderer);
    event_loop.run_loop();

    t1.join();
}

TEST_CASE("AudioRenderGraph_test_bad") {
    // Generate a render stage graph

    auto audio_generator = new AudioSingleShaderFileGeneratorRenderStage(512, 44100, 2, "media/test.wav");
    auto effect_render_stage = new AudioGainEffectRenderStage(512, 44100, 2);

    auto audio_generator_2 = new AudioSingleShaderFileGeneratorRenderStage(512, 44100, 2, "media/test.wav");
    auto effect_render_stage_2 = new AudioGainEffectRenderStage(512, 44100, 2);

    auto join_render_stage = new AudioMultitrackJoinRenderStage(512, 44100, 2, 2);
    auto final_render_stage = new AudioFinalRenderStage(512, 44100, 2);

    // Get the parameters from the audio_generator render stage
    audio_generator->connect_render_stage(effect_render_stage);
    audio_generator_2->connect_render_stage(effect_render_stage_2);

    REQUIRE(!audio_generator->connect_render_stage(effect_render_stage_2));

    effect_render_stage->connect_render_stage(join_render_stage);
    effect_render_stage_2->connect_render_stage(join_render_stage);

    REQUIRE(!audio_generator->connect_render_stage(effect_render_stage));
    REQUIRE(effect_render_stage_2->disconnect_render_stage(join_render_stage));

    join_render_stage->connect_render_stage(final_render_stage);

    REQUIRE_THROWS_AS(AudioRenderGraph({audio_generator, audio_generator_2}), std::runtime_error);
}

TEST_CASE("AudioRenderGraph_modify_graph") {
    // Generate a render stage graph
    auto audio_generator = new AudioSingleShaderFileGeneratorRenderStage(512, 44100, 2, "media/test.wav");
    auto final_render_stage = new AudioFinalRenderStage(512, 44100, 2);
    auto effect_render_stage = new AudioGainEffectRenderStage(512, 44100, 2);
    auto effect_render_stage_2 = new AudioGainEffectRenderStage(512, 44100, 2);

    audio_generator->connect_render_stage(final_render_stage);

    auto graph = new AudioRenderGraph({audio_generator});
    REQUIRE(graph != nullptr);

    AudioRenderer& audio_renderer = AudioRenderer::get_instance();
    audio_renderer.add_render_graph(graph);

    auto audio_driver = new AudioPlayerOutput(512, 44100, 2);
    audio_renderer.add_render_output(audio_driver);

    EventLoop& event_loop = EventLoop::get_instance();

    // Open a thread to modify the graph and terminate the event loop
    std::thread t1([&audio_renderer, &audio_generator, &graph, &effect_render_stage, &effect_render_stage_2, &event_loop]() {
        auto time_param = audio_renderer.find_global_parameter("global_time");

        auto position_param = audio_generator->find_parameter("play_position");
        auto play_param = audio_generator->find_parameter("gain");

        play_param->set_value(0.0f);

        std::this_thread::sleep_for(std::chrono::seconds(1));
        // Play audio
        position_param->set_value(time_param->get_value());
        play_param->set_value(1.0f);

        std::this_thread::sleep_for(std::chrono::seconds(1));
        // Modify the graph
        graph->insert_render_stage_behind(audio_generator->gid, effect_render_stage);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        graph->insert_render_stage_infront(effect_render_stage->gid, effect_render_stage_2);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto removed = graph->remove_render_stage(effect_render_stage->gid);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        graph->replace_render_stage(effect_render_stage_2->gid, removed.get());
        std::this_thread::sleep_for(std::chrono::seconds(1));

        event_loop.terminate();
    });

    REQUIRE(audio_renderer.initialize(512, 44100, 2));
    REQUIRE(audio_driver->open());
    REQUIRE(audio_driver->start());

    event_loop.add_loop_item(&audio_renderer);
    event_loop.run_loop();

    t1.join();
}


