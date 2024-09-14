#include "catch2/catch_all.hpp"
#include "catch2/catch_approx.hpp"
#include <thread>

#include "audio_renderer.h"
#include "audio_player_output.h"
#include "audio_texture2d_parameter.h"

TEST_CASE("AudioRenderer") {
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    
    AudioRenderStage render_stage2 = AudioRenderStage(512, 44100, 2);
    float * audio_buffer = new float[512*2]();
    std::fill_n(audio_buffer, 512*2, -0.1f);

    const float * empty_buffer = new float[512*2]();

    // Create parameters for the audio generator ports in shader
    auto input_audio_texture =
        std::make_unique<AudioTexture2DParameter>("input_audio_texture",
                              AudioParameter::ConnectionType::INPUT,
                              512 * 2, 1);
    REQUIRE(input_audio_texture->set_value(audio_buffer));

    auto output_audio_texture =
        std::make_unique<AudioTexture2DParameter>("output_audio_texture",
                              AudioParameter::ConnectionType::OUTPUT,
                              512 * 2, 1);

    REQUIRE(render_stage2.update_audio_buffer(audio_buffer, 512*2));
    const char * m_fragment_source2 = R"glsl(
        #version 300 es
        precision highp float;

        in vec2 TexCoord;
        out vec4 output_audio_texture;

        uniform sampler2D input_audio_texture;

        void main()
        {
            float color = texture(input_audio_texture, TexCoord).r + 0.01f;
            float color2 = 0.001;
            output_audio_texture = vec4(color+color2, color2, 0.0, 1.0);
        }
    )glsl";
    REQUIRE(render_stage2.update_fragment_source(m_fragment_source2));
    audio_renderer.add_render_stage(render_stage2);

    AudioRenderStage render_stage3 = AudioRenderStage(512, 44100, 2);
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
    
    auto output_audio_texture2 =
        std::make_unique<AudioTexture2DParameter>("output_audio_texture",
                              AudioParameter::ConnectionType::OUTPUT,
                              512 * 2, 1);

    REQUIRE(render_stage3.update_audio_buffer(audio_buffer2, 512*2));
    const char * m_fragment_source3 = R"glsl(
        #version 300 es
        precision highp float;
        in vec2 TexCoord;
        out vec4 output_audio_texture;
        uniform sampler2D input_audio_texture;
        uniform sampler2D stream_audio_texture;
        void main()
        {
            float color = texture(input_audio_texture, TexCoord).r + 0.02f;
            float color2 = texture(stream_audio_texture, TexCoord).r + 0.002f;
            output_audio_texture = vec4(color+color2, 0.0, color2, 1.0);
        }
    )glsl";
    REQUIRE(render_stage3.update_fragment_source(m_fragment_source3));
    audio_renderer.add_render_stage(render_stage3);

    REQUIRE(output_audio_texture->link_to_parameter(stream_audio_texture2.get()));
    REQUIRE(render_stage2.add_parameter(std::move(input_audio_texture)));
    REQUIRE(render_stage2.add_parameter(std::move(output_audio_texture)));
    REQUIRE(render_stage3.add_parameter(std::move(input_audio_texture2)));
    REQUIRE(render_stage3.add_parameter(std::move(stream_audio_texture2)));
    REQUIRE(render_stage3.add_parameter(std::move(output_audio_texture2)));


    //AudioRenderStage render_stage5 = AudioRenderStage(512, 44100, 2);
    //float * audio_buffer4 = new float[512*2]();
    //std::fill_n(audio_buffer4, 512*2, -0.3f);

    //// Create parameters for the audio generator ports in shader
    //auto input_audio_texture4 =
    //    std::make_unique<AudioTexture2DParameter>("input_audio_texture",
    //                          AudioParameter::ConnectionType::INPUT,
    //                          512, 2);
    //input_audio_texture4->set_value(audio_buffer4);

    //auto stream_audio_texture4 = 
    //    std::make_unique<AudioTexture2DParameter>("stream_audio_texture",
    //                          AudioParameter::ConnectionType::PASSTHROUGH,
    //                          512, 2);
    
    //auto output_audio_texture4 =
    //    std::make_unique<AudioTexture2DParameter>("output_audio_texture",
    //                          AudioParameter::ConnectionType::OUTPUT,
    //                          512, 2);

    //REQUIRE(render_stage5.add_parameter(std::move(input_audio_texture4)));
    //REQUIRE(render_stage5.add_parameter(std::move(stream_audio_texture4)));
    //REQUIRE(render_stage5.add_parameter(std::move(output_audio_texture4)));

    //REQUIRE(render_stage5.update_audio_buffer(audio_buffer4, 512*2));
    //const char * m_fragment_source4 = R"glsl(
    //    #version 300 es
    //    precision highp float;
    //    in vec2 TexCoord;
    //    out vec4 output_audio_texture;
    //    uniform sampler2D input_audio_texture;
    //    uniform sampler2D stream_audio_texture;
    //    void main()
    //    {
    //        float color = texture(input_audio_texture, TexCoord).r + 0.03f;
    //        float color2 = texture(stream_audio_texture, TexCoord).r + 0.003f;
    //        output_audio_texture = vec4(color+color2, color2+color, color2+color, 1.0);
    //    }
    //)glsl";
    //REQUIRE(render_stage5.update_fragment_source(m_fragment_source4));
    //audio_renderer.add_render_stage(render_stage5);


    REQUIRE(audio_renderer.init(512, 44100, 2));

    // Open a thread to wait 1 sec and the shut it down
    std::thread t1([&audio_renderer](){
        // Wait for 1 sec
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        audio_renderer.terminate();
    });

    // Check the output buffer data
    AudioBuffer * output_buffer = audio_renderer.get_new_output_buffer();

    output_buffer->push(new float[512*2](), 512*2);
    audio_renderer.iterate();
    output_buffer->push(new float[512*2](), 512*2); // Need at least one more in buffer

    auto buffer = output_buffer->pop();
    for (int j = 0; j < 512*2; j++) {
        REQUIRE(buffer[j] == Catch::Approx(0.0f));
    }

    buffer = output_buffer->pop();
    for (int i = 0; i < 512*2; i++) {
        //REQUIRE(buffer[i] == Catch::Approx(-0.1f - 0.2f - 0.3f + 0.01f + 0.02f + 0.03f + 0.001f + 0.002f + 0.003f));
        REQUIRE(buffer[i] == Catch::Approx(-0.1f - 0.2f + 0.01f + 0.02f + 0.001f + 0.002f));
    }

    t1.detach();
    audio_renderer.main_loop();
}