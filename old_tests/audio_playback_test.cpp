#include "catch2/catch_all.hpp"
#include <thread>
#include <iostream>
#include <mutex>

#include "audio_core/audio_renderer.h"
#include "audio_render_stage/audio_tape_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_core/audio_render_graph.h"
#include "audio_output/audio_player_output.h"
#include "engine/event_loop.h"

TEST_CASE("AudioPlaybackRenderStage_test_empty_tape") {
    auto audio_playback_render_stage = new AudioPlaybackRenderStage(512, 44100, 2);
    auto audio_final_render_stage = new AudioFinalRenderStage(512, 44100, 2);

    audio_playback_render_stage->connect_render_stage(audio_final_render_stage);

    auto audio_render_graph = new AudioRenderGraph({audio_final_render_stage});

    auto audio_driver = new AudioPlayerOutput(512, 44100, 2);

    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    EventLoop & event_loop = EventLoop::get_instance();

    audio_playback_render_stage->load_tape({});

    audio_renderer.add_render_graph(audio_render_graph);
    audio_renderer.add_render_output(audio_driver);

    // Open a thread to control playback and terminate the event loop
    std::thread t1([&audio_renderer, &audio_playback_render_stage, &event_loop]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        audio_playback_render_stage->play(0);
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

TEST_CASE("AudioPlaybackRenderStage_test_small_tape") {
    auto audio_playback_render_stage = new AudioPlaybackRenderStage(512, 44100, 2);
    auto audio_final_render_stage = new AudioFinalRenderStage(512, 44100, 2);

    audio_playback_render_stage->connect_render_stage(audio_final_render_stage);

    auto audio_render_graph = new AudioRenderGraph({audio_final_render_stage});

    auto audio_driver = new AudioPlayerOutput(512, 44100, 2);

    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    EventLoop & event_loop = EventLoop::get_instance();

    audio_playback_render_stage->load_tape({0.0f, 1.0f, 0.0f, 1.0f});

    audio_renderer.add_render_graph(audio_render_graph);
    audio_renderer.add_render_output(audio_driver);

    // Open a thread to control playback and terminate the event loop
    std::thread t1([&audio_renderer, &audio_playback_render_stage, &event_loop]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        audio_playback_render_stage->play(1);
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

TEST_CASE("AudioPlaybackRenderStage_test_large_tape") {
    auto audio_playback_render_stage = new AudioPlaybackRenderStage(512, 44100, 2);
    auto audio_final_render_stage = new AudioFinalRenderStage(512, 44100, 2);

    audio_playback_render_stage->connect_render_stage(audio_final_render_stage);

    auto audio_render_graph = new AudioRenderGraph({audio_final_render_stage});

    auto audio_driver = new AudioPlayerOutput(512, 44100, 2);

    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    EventLoop & event_loop = EventLoop::get_instance();

    auto tape = std::vector<float>(44100 * 5 + 12, 1.0f);
    audio_playback_render_stage->load_tape(tape);

    audio_renderer.add_render_graph(audio_render_graph);
    audio_renderer.add_render_output(audio_driver);

    // Open a thread to control playback and terminate the event loop
    std::thread t1([&audio_renderer, &audio_playback_render_stage, &event_loop]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        audio_playback_render_stage->play(2);
        std::this_thread::sleep_for(std::chrono::seconds(3));
        event_loop.terminate();
    });

    REQUIRE(audio_renderer.initialize(512, 44100, 2));
    REQUIRE(audio_driver->open());
    REQUIRE(audio_driver->start());

    event_loop.add_loop_item(&audio_renderer);
    event_loop.run_loop();

    t1.join();
}