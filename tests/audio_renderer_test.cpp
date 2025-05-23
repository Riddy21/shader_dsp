#include "catch2/catch_all.hpp"
#include "catch2/catch_approx.hpp"
#include <thread>

#include "audio_core/audio_renderer.h"
#include "audio_core/audio_render_graph.h"
#include "audio_output/audio_player_output.h"
#include "audio_parameter/audio_texture2d_parameter.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "engine/event_loop.h"

TEST_CASE("AudioRenderer") {
    EventLoop & event_loop = EventLoop::get_instance();
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    
    auto render_stage2 = new AudioRenderStage(512, 44100, 2);
    float * audio_buffer = new float[512*2]();
    std::fill_n(audio_buffer, 512*2, -0.1f);

    float * empty_buffer = new float[512*2]();

    auto stream_audio_texture = 
        new AudioTexture2DParameter("stream_audio_texture",
                              AudioParameter::ConnectionType::INPUT,
                              512 * 2, 1);
    REQUIRE(stream_audio_texture->set_value(empty_buffer));

    auto output_audio_texture =
        new AudioTexture2DParameter("output_audio_texture",
                              AudioParameter::ConnectionType::OUTPUT,
                              512 * 2, 1);

    auto render_stage3 = new AudioRenderStage(512, 44100, 2);
    float * audio_buffer2 = new float[512*2]();
    std::fill_n(audio_buffer2, 512*2, -0.2f);

    auto stream_audio_texture2 = 
        new AudioTexture2DParameter("stream_audio_texture",
                              AudioParameter::ConnectionType::PASSTHROUGH,
                              512 * 2, 1);
    
    auto output_audio_texture2 =
        new AudioTexture2DParameter("output_audio_texture",
                              AudioParameter::ConnectionType::OUTPUT,
                              512 * 2, 1);

    auto render_stage5 = new AudioRenderStage(512, 44100, 2);
    float * audio_buffer4 = new float[512*2]();
    std::fill_n(audio_buffer4, 512*2, -0.3f);

    auto stream_audio_texture4 = 
        new AudioTexture2DParameter("stream_audio_texture",
                              AudioParameter::ConnectionType::PASSTHROUGH,
                              512 * 2, 1);
    
    auto output_audio_texture4 =
        new AudioTexture2DParameter("output_audio_texture",
                              AudioParameter::ConnectionType::OUTPUT,
                              512 * 2, 1);

    REQUIRE(output_audio_texture->link(stream_audio_texture2));
    REQUIRE(output_audio_texture2->link(stream_audio_texture4));
    REQUIRE(render_stage2->add_parameter(stream_audio_texture));
    REQUIRE(render_stage2->add_parameter(output_audio_texture));
    REQUIRE(render_stage3->add_parameter(stream_audio_texture2));
    REQUIRE(render_stage3->add_parameter(output_audio_texture2));
    REQUIRE(render_stage5->add_parameter(stream_audio_texture4));
    REQUIRE(render_stage5->add_parameter(output_audio_texture4));

    auto final_render_stage = new AudioFinalRenderStage(512, 44100, 2);

    output_audio_texture4->link(final_render_stage->find_parameter("stream_audio_texture"));

    auto audio_render_graph = new AudioRenderGraph(final_render_stage);

    audio_renderer.add_render_graph(audio_render_graph);

    REQUIRE(audio_renderer.initialize(512, 44100, 2));

    event_loop.add_loop_item(&audio_renderer);

    // Open a thread to wait 1 sec and the shut it down
    std::thread t1([&audio_renderer, &event_loop](){
        // Wait for 1 sec
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        event_loop.terminate();
    });

    event_loop.run_loop();
    t1.join();
}