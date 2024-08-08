#include "catch2/catch_all.hpp"
#include "catch2/catch_approx.hpp"
#include <thread>

#include "audio_renderer.h"
#include "audio_driver.h"

TEST_CASE("AudioRenderer") {
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    
    // TODO: Add a render stage and see if it works

    AudioRenderStage render_stage2 = AudioRenderStage(512, 44100, 20);
    render_stage2.audio_buffer = std::vector<float>(512*20, 0.1f);
    render_stage2.fragment_source = R"glsl(
        #version 300 es
        precision highp float;
        in vec2 TexCoord;
        out vec4 FragColor;
        uniform sampler2D input_audio_texture;
        uniform sampler2D stream_audio_texture;
        void main()
        {
            float color = texture(input_audio_texture, TexCoord).r + 0.01f;
            float color2 = texture(stream_audio_texture, TexCoord).r + 0.001;
            FragColor = vec4(color+color2, color2, 0.0, 1.0);
        }
    )glsl";
    audio_renderer.add_render_stage(render_stage2);

    AudioRenderStage render_stage3 = AudioRenderStage(512, 44100, 20);
    render_stage3.audio_buffer = std::vector<float>(512*20, 0.2f);
    render_stage3.fragment_source = R"glsl(
        #version 300 es
        precision highp float;
        in vec2 TexCoord;
        out vec4 FragColor;
        uniform sampler2D input_audio_texture;
        uniform sampler2D stream_audio_texture;
        void main()
        {
            float color = texture(input_audio_texture, TexCoord).r + 0.02f;
            float color2 = texture(stream_audio_texture, TexCoord).r + 0.002;
            FragColor = vec4(color+color2, 0.0, color2, 1.0);
        }
    )glsl";
    audio_renderer.add_render_stage(render_stage3);

    AudioRenderStage render_stage4 = AudioRenderStage(512, 44100, 20);
    render_stage4.audio_buffer = std::vector<float>(512*20, 0.3f);
    render_stage4.fragment_source = R"glsl(
        #version 300 es
        precision highp float;
        in vec2 TexCoord;
        out vec4 FragColor;
        uniform sampler2D input_audio_texture;
        uniform sampler2D stream_audio_texture;
        void main()
        {
            float color = texture(input_audio_texture, TexCoord).r + 0.03f;
            float color2 = texture(stream_audio_texture, TexCoord).r + 0.003f;
            FragColor = vec4(color+color2, color2, color, 1.0);
        }
    )glsl";
    audio_renderer.add_render_stage(render_stage4);

    AudioRenderStage render_stage5 = AudioRenderStage(512, 44100, 20);
    render_stage5.audio_buffer = std::vector<float>(512*20, 0.4f);
    render_stage5.fragment_source = R"glsl(
        #version 300 es
        precision highp float;
        in vec2 TexCoord;
        out vec4 FragColor;
        uniform sampler2D input_audio_texture;
        uniform sampler2D stream_audio_texture;
        void main()
        {
            float color = texture(input_audio_texture, TexCoord).r + 0.04f;
            float color2 = texture(stream_audio_texture, TexCoord).r + 0.004f;
            FragColor = vec4(color+color2, color2, color, 1.0);
        }
    )glsl";
    audio_renderer.add_render_stage(render_stage5);


    REQUIRE(audio_renderer.init(512, 20));

    // Open a thread to wait 1 sec and the shut it down
    std::thread t1([&audio_renderer](){
        // Wait for 1 sec
        std::this_thread::sleep_for(std::chrono::seconds(1));
        audio_renderer.terminate();
    });

    t1.detach();

    audio_renderer.main_loop();

    // Check the output buffer data
    std::vector<float> output_buffer_data = audio_renderer.get_output_buffer_data();
    for (int i = 0; i < 512*20; i++) {
        REQUIRE(output_buffer_data[i] == Catch::Approx(1.21f));
    }
}