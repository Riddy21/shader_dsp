#include "catch2/catch_all.hpp"
#include <thread>
#include <iostream>
#include <mutex>

#include "audio_output/audio_player_output.h"
#include "audio_core/audio_renderer.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_render_stage/audio_file_generator_render_stage.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_core/audio_render_graph.h"
#include "engine/event_loop.h"

TEST_CASE("AudioGeneratorRenderStage") {
    std::vector<std::string> file_paths = {
        "build/shaders/sawtooth_generator_render_stage.glsl",
        "build/shaders/triangle_generator_render_stage.glsl",
        "build/shaders/square_generator_render_stage.glsl",
        "build/shaders/sine_generator_render_stage.glsl",
        "build/shaders/static_generator_render_stage.glsl",
        "media/test.wav"
    };

    for (const auto& file_path : file_paths) {
        auto audio_generator = (file_path == "media/test.wav") ?
            static_cast<AudioSingleShaderGeneratorRenderStage*>(new AudioSingleShaderFileGeneratorRenderStage(512, 44100, 2, file_path)) :
            new AudioSingleShaderGeneratorRenderStage(512, 44100, 2, file_path);

        auto audio_final_render_stage = new AudioFinalRenderStage(512, 44100, 2);

        audio_generator->connect_render_stage(audio_final_render_stage);

        auto audio_render_graph = new AudioRenderGraph({audio_final_render_stage});

        auto audio_driver = new AudioPlayerOutput(512, 44100, 2);

        AudioRenderer & audio_renderer = AudioRenderer::get_instance();

        auto play_param = audio_generator->find_parameter("play");
        auto gain_param = audio_generator->find_parameter("gain");
        auto play_position_param = audio_generator->find_parameter("play_position");
        auto stop_position_param = audio_generator->find_parameter("stop_position");
        auto time_param = audio_renderer.find_global_parameter("global_time");

        gain_param->set_value(0.2f);

        audio_renderer.add_render_graph(audio_render_graph);
        audio_renderer.add_render_output(audio_driver);

        EventLoop & event_loop = EventLoop::get_instance();

        // Open a thread to wait few sec and the shut it down
        std::thread t1([&audio_renderer, &play_param, &play_position_param, &stop_position_param, &time_param, &event_loop]() {
            // Play the audio
            play_param->set_value(true);
            play_position_param->set_value(time_param->get_value());
            std::this_thread::sleep_for(std::chrono::seconds(3));
            play_param->set_value(false);
            stop_position_param->set_value(time_param->get_value());

            event_loop.terminate();
        });

        REQUIRE(audio_renderer.initialize(512, 44100, 2));

        REQUIRE(audio_driver->open());
        REQUIRE(audio_driver->start());

        event_loop.add_loop_item(&audio_renderer);
        event_loop.run_loop();

        t1.detach();
        t1.join();
    }
}

TEST_CASE("AudioMultitoneGeneratorRenderStage") {
    std::vector<std::string> file_paths = {
        "build/shaders/multinote_square_generator_render_stage.glsl",
        "build/shaders/multinote_sawtooth_generator_render_stage.glsl",
        "build/shaders/multinote_triangle_generator_render_stage.glsl",
        "build/shaders/multinote_sine_generator_render_stage.glsl",
        "build/shaders/multinote_static_generator_render_stage.glsl",
        "media/test.wav"
    };

    for (const auto& file_path : file_paths) {
        //auto audio_generator = (file_path == "media/test.wav") ?
        //    static_cast<AudioSingleShaderGeneratorRenderStage*>(new AudioSingleShaderFileGeneratorRenderStage(512, 44100, 2, file_path)) :
        auto audio_generator = (file_path == "media/test.wav") ?
            static_cast<AudioGeneratorRenderStage*>(new AudioFileGeneratorRenderStage(512, 44100, 2, file_path)) :
            new AudioGeneratorRenderStage(512, 44100, 2, file_path);

        auto audio_final_render_stage = new AudioFinalRenderStage(512, 44100, 2);

        audio_generator->connect_render_stage(audio_final_render_stage);

        auto audio_render_graph = new AudioRenderGraph({audio_final_render_stage});

        auto audio_driver = new AudioPlayerOutput(512, 44100, 2);

        AudioRenderer & audio_renderer = AudioRenderer::get_instance();

        audio_renderer.add_render_graph(audio_render_graph);
        audio_renderer.add_render_output(audio_driver);

        EventLoop & event_loop = EventLoop::get_instance();

        // Open a thread to wait few sec and the shut it down
        std::thread t1([&audio_generator, &audio_renderer, &event_loop]() {

            auto play_position_param = audio_generator->find_parameter("play_positions");
            auto stop_position_param = audio_generator->find_parameter("stop_positions");
            auto tone_param = audio_generator->find_parameter("tones");
            auto gain_param = audio_generator->find_parameter("gains");
            auto active_notes = audio_generator->find_parameter("active_notes");

            auto time_param = audio_renderer.find_global_parameter("global_time");

            audio_generator->play_note(MIDDLE_C, 0.2f);

            audio_generator->play_note(MIDDLE_C * std::pow(SEMI_TONE, 1), 0.2f);

            std::this_thread::sleep_for(std::chrono::seconds(3));

            audio_generator->stop_note(MIDDLE_C);

            std::this_thread::sleep_for(std::chrono::seconds(3));

            audio_generator->stop_note(MIDDLE_C * std::pow(SEMI_TONE, 1));

            std::this_thread::sleep_for(std::chrono::seconds(3));

            // terminate
            event_loop.terminate();
        });

        REQUIRE(audio_renderer.initialize(512, 44100, 2));

        REQUIRE(audio_driver->open());
        REQUIRE(audio_driver->start());

        event_loop.add_loop_item(&audio_renderer);
        event_loop.run_loop();

        t1.detach();
        t1.join();
    }
}
