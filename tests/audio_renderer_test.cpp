#include "catch2/catch_all.hpp"
#include "catch2/catch_approx.hpp"
#include <thread>

#include "audio_renderer.h"
#include "audio_player_output.h"
#include "audio_file_output.h"
#include "audio_texture2d_parameter.h"
#include "audio_uniform_parameters.h"

TEST_CASE("AudioRenderer") {
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    
    auto render_stage2 = std::make_unique<AudioRenderStage>(512, 44100, 2);
    float * audio_buffer = new float[512*2]();
    std::fill_n(audio_buffer, 512*2, -0.1f);

    float * empty_buffer = new float[512*2]();

    // Create parameters for the audio generator ports in shader
    auto input_audio_texture =
        std::make_unique<AudioTexture2DParameter>("input_audio_texture",
                              AudioParameter::ConnectionType::INPUT,
                              512 * 2, 1);
    REQUIRE(input_audio_texture->set_value(audio_buffer));

    auto stream_audio_texture = 
        std::make_unique<AudioTexture2DParameter>("stream_audio_texture",
                              AudioParameter::ConnectionType::INPUT,
                              512 * 2, 1);
    REQUIRE(stream_audio_texture->set_value(empty_buffer));

    auto time_param =
        std::make_unique<AudioIntParameter>("time",
                              AudioParameter::ConnectionType::INPUT);
    REQUIRE(time_param->set_value(new int(0)));

    auto output_audio_texture =
        std::make_unique<AudioTexture2DParameter>("output_audio_texture",
                              AudioParameter::ConnectionType::OUTPUT,
                              512 * 2, 1);

    auto render_stage3 = std::make_unique<AudioRenderStage>(512, 44100, 2);
    float * audio_buffer2 = new float[512*2]();
    std::fill_n(audio_buffer2, 512*2, -0.2f);

    // Create parameters for the audio generator ports in shader
    auto input_audio_texture2 =
        std::make_unique<AudioTexture2DParameter>("input_audio_texture",
                              AudioParameter::ConnectionType::INPUT,
                              512 * 2, 1);
    REQUIRE(input_audio_texture2->set_value(audio_buffer2));

    auto stream_audio_texture2 = 
        std::make_unique<AudioTexture2DParameter>("stream_audio_texture",
                              AudioParameter::ConnectionType::PASSTHROUGH,
                              512 * 2, 1);
 
    auto time_param2 =
        std::make_unique<AudioIntParameter>("time",
                              AudioParameter::ConnectionType::INPUT);
    REQUIRE(time_param2->set_value(new int(0)));

    
    auto output_audio_texture2 =
        std::make_unique<AudioTexture2DParameter>("output_audio_texture",
                              AudioParameter::ConnectionType::OUTPUT,
                              512 * 2, 1);

    auto render_stage5 = std::make_unique<AudioRenderStage>(512, 44100, 2);
    float * audio_buffer4 = new float[512*2]();
    std::fill_n(audio_buffer4, 512*2, -0.3f);

    // Create parameters for the audio generator ports in shader
    auto input_audio_texture4 =
        std::make_unique<AudioTexture2DParameter>("input_audio_texture",
                              AudioParameter::ConnectionType::INPUT,
                              512 * 2, 1);
    input_audio_texture4->set_value(audio_buffer4);

    auto stream_audio_texture4 = 
        std::make_unique<AudioTexture2DParameter>("stream_audio_texture",
                              AudioParameter::ConnectionType::PASSTHROUGH,
                              512 * 2, 1);

    auto time_param4 =
        std::make_unique<AudioIntParameter>("time",
                              AudioParameter::ConnectionType::INPUT);
    REQUIRE(time_param4->set_value(new int(0)));

    
    auto output_audio_texture4 =
        std::make_unique<AudioTexture2DParameter>("output_audio_texture",
                              AudioParameter::ConnectionType::OUTPUT,
                              512 * 2, 1);


    REQUIRE(output_audio_texture->link(stream_audio_texture2.get()));
    REQUIRE(output_audio_texture2->link(stream_audio_texture4.get()));
    REQUIRE(render_stage2->add_parameter(std::move(input_audio_texture)));
    REQUIRE(render_stage2->add_parameter(std::move(time_param)));
    REQUIRE(render_stage2->add_parameter(std::move(stream_audio_texture)));
    REQUIRE(render_stage2->add_parameter(std::move(output_audio_texture)));
    REQUIRE(render_stage3->add_parameter(std::move(time_param2)));
    REQUIRE(render_stage3->add_parameter(std::move(input_audio_texture2)));
    REQUIRE(render_stage3->add_parameter(std::move(stream_audio_texture2)));
    REQUIRE(render_stage3->add_parameter(std::move(output_audio_texture2)));
    REQUIRE(render_stage5->add_parameter(std::move(time_param4)));
    REQUIRE(render_stage5->add_parameter(std::move(input_audio_texture4)));
    REQUIRE(render_stage5->add_parameter(std::move(stream_audio_texture4)));
    REQUIRE(render_stage5->add_parameter(std::move(output_audio_texture4)));

    audio_renderer.add_render_stage(std::move(render_stage2));
    audio_renderer.add_render_stage(std::move(render_stage3));
    audio_renderer.add_render_stage(std::move(render_stage5));

    REQUIRE(audio_renderer.init(512, 44100, 2));
    audio_renderer.set_testing_mode(true);

    // Open a thread to wait 1 sec and the shut it down
    std::thread t1([&audio_renderer](){
        // Wait for 1 sec
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        audio_renderer.terminate();
    });

    // Check the output buffer data
    AudioBuffer * output_buffer = audio_renderer.get_new_output_buffer();


    output_buffer->push(new float[512*2](), true);
    audio_renderer.iterate();
    output_buffer->push(new float[512*2](), true); // Need at least one more in buffer

    auto buffer = output_buffer->pop(true);
    for (int j = 0; j < 512*2; j++) {
        REQUIRE(buffer[j] == Catch::Approx(0.0f));
    }

    buffer = output_buffer->pop();
    for (int i = 0; i < 512*2; i++) {
        REQUIRE(buffer[i] == Catch::Approx(-0.1f - 0.2f - 0.3f));
    }

    t1.detach();
    audio_renderer.main_loop();
}