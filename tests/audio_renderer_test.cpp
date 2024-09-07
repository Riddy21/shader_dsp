#include "catch2/catch_all.hpp"
#include "catch2/catch_approx.hpp"
#include <thread>

#include "audio_renderer.h"
#include "audio_player_output.h"

TEST_CASE("AudioRenderer") {
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    
    // TODO: Add a render stage and see if it works

    AudioRenderStage render_stage2 = AudioRenderStage(512, 44100, 2);
    float audio_buffer[512*2];
    std::fill_n(audio_buffer, 512*2, -0.1f);

    const float * empty_buffer = new float[512*2]();

    // Create parameters for the audio generator ports in shader
    AudioRenderStageParameter input_audio_texture_parameter;
    input_audio_texture_parameter.name = "input_audio_texture";
    input_audio_texture_parameter.type = AudioRenderStageParameter::Type::STREAM_INPUT;
    input_audio_texture_parameter.parameter_width = 512 * 2;
    input_audio_texture_parameter.parameter_height = 1;
    input_audio_texture_parameter.data = &render_stage2.m_audio_buffer;

    AudioRenderStageParameter stream_audio_texture_parameter;
    stream_audio_texture_parameter.name = "stream_audio_texture";
    stream_audio_texture_parameter.link_name = nullptr;
    stream_audio_texture_parameter.type = AudioRenderStageParameter::Type::STREAM_INPUT;
    stream_audio_texture_parameter.parameter_width = 512 * 2;
    stream_audio_texture_parameter.parameter_height = 1;
    stream_audio_texture_parameter.data = &empty_buffer;

    AudioRenderStageParameter output_audio_texture_parameter;
    output_audio_texture_parameter.name = "output_audio_texture";
    output_audio_texture_parameter.link_name = "stream_audio_texture";
    output_audio_texture_parameter.type = AudioRenderStageParameter::Type::STREAM_OUTPUT;
    output_audio_texture_parameter.parameter_width = 512 * 2;
    output_audio_texture_parameter.parameter_height = 1;
    output_audio_texture_parameter.data = nullptr;

    REQUIRE(render_stage2.add_parameter(input_audio_texture_parameter));
    REQUIRE(render_stage2.add_parameter(stream_audio_texture_parameter));
    REQUIRE(render_stage2.add_parameter(output_audio_texture_parameter));

    render_stage2.m_audio_buffer = audio_buffer;
    render_stage2.m_fragment_source = R"glsl(
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

    AudioRenderStage render_stage3 = AudioRenderStage(512, 44100, 2);
    float audio_buffer2[512*2];
    std::fill_n(audio_buffer2, 512*2, -0.2f);

    // Create parameters for the audio generator ports in shader
    AudioRenderStageParameter input_audio_texture_parameter2;
    input_audio_texture_parameter2.name = "input_audio_texture";
    input_audio_texture_parameter2.type = AudioRenderStageParameter::Type::STREAM_INPUT;
    input_audio_texture_parameter2.parameter_width = 512 * 2;
    input_audio_texture_parameter2.parameter_height = 1;
    input_audio_texture_parameter2.data = &render_stage3.m_audio_buffer;

    AudioRenderStageParameter stream_audio_texture_parameter2;
    stream_audio_texture_parameter2.name = "stream_audio_texture";
    stream_audio_texture_parameter2.link_name = "output_audio_texture";
    stream_audio_texture_parameter2.type = AudioRenderStageParameter::Type::STREAM_INPUT;
    stream_audio_texture_parameter2.parameter_width = 512 * 2;
    stream_audio_texture_parameter2.parameter_height = 1;
    stream_audio_texture_parameter2.data = nullptr;

    AudioRenderStageParameter output_audio_texture_parameter2;
    output_audio_texture_parameter2.name = "output_audio_texture";
    output_audio_texture_parameter2.link_name = "stream_audio_texture";
    output_audio_texture_parameter2.type = AudioRenderStageParameter::Type::STREAM_OUTPUT;  
    output_audio_texture_parameter2.parameter_width = 512 * 2;
    output_audio_texture_parameter2.parameter_height = 1;
    output_audio_texture_parameter2.data = nullptr;

    render_stage3.add_parameter(input_audio_texture_parameter2);
    render_stage3.add_parameter(stream_audio_texture_parameter2);
    render_stage3.add_parameter(output_audio_texture_parameter2);

    render_stage3.m_audio_buffer = audio_buffer2;
    render_stage3.m_fragment_source = R"glsl(
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

    AudioRenderStage render_stage4 = AudioRenderStage(512, 44100, 2);
    float audio_buffer3[512*2];
    std::fill_n(audio_buffer3, 512*2, -0.3f);

    // Create parameters for the audio generator ports in shader
    AudioRenderStageParameter input_audio_texture_parameter3;
    input_audio_texture_parameter3.name = "input_audio_texture";
    input_audio_texture_parameter3.type = AudioRenderStageParameter::Type::STREAM_INPUT;
    input_audio_texture_parameter3.parameter_width = 512 * 2;
    input_audio_texture_parameter3.parameter_height = 1;
    input_audio_texture_parameter3.data = &render_stage4.m_audio_buffer;

    AudioRenderStageParameter stream_audio_texture_parameter3;
    stream_audio_texture_parameter3.name = "stream_audio_texture";
    stream_audio_texture_parameter3.link_name = "output_audio_texture";
    stream_audio_texture_parameter3.type = AudioRenderStageParameter::Type::STREAM_INPUT;
    stream_audio_texture_parameter3.parameter_width = 512 * 2;
    stream_audio_texture_parameter3.parameter_height = 1;
    stream_audio_texture_parameter3.data = nullptr;

    AudioRenderStageParameter output_audio_texture_parameter3;
    output_audio_texture_parameter3.name = "output_audio_texture";
    output_audio_texture_parameter3.link_name = "stream_audio_texture";
    output_audio_texture_parameter3.type = AudioRenderStageParameter::Type::STREAM_OUTPUT;
    output_audio_texture_parameter3.parameter_width = 512 * 2;
    output_audio_texture_parameter3.parameter_height = 1;
    output_audio_texture_parameter3.data = nullptr;

    render_stage4.add_parameter(input_audio_texture_parameter3);
    render_stage4.add_parameter(stream_audio_texture_parameter3);
    render_stage4.add_parameter(output_audio_texture_parameter3);

    render_stage4.m_audio_buffer = audio_buffer3;
    render_stage4.m_fragment_source = R"glsl(
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

    AudioRenderStage render_stage5 = AudioRenderStage(512, 44100, 2);
    float audio_buffer4[512*2];
    std::fill_n(audio_buffer4, 512*2, -0.4f);

    // Create parameters for the audio generator ports in shader
    AudioRenderStageParameter input_audio_texture_parameter4;
    input_audio_texture_parameter4.name = "input_audio_texture";
    input_audio_texture_parameter4.type = AudioRenderStageParameter::Type::STREAM_INPUT;
    input_audio_texture_parameter4.parameter_width = 512 * 2;
    input_audio_texture_parameter4.parameter_height = 1;
    input_audio_texture_parameter4.data = &render_stage5.m_audio_buffer;

    AudioRenderStageParameter stream_audio_texture_parameter4;
    stream_audio_texture_parameter4.name = "stream_audio_texture";
    stream_audio_texture_parameter4.link_name = "output_audio_texture";
    stream_audio_texture_parameter4.type = AudioRenderStageParameter::Type::STREAM_INPUT;
    stream_audio_texture_parameter4.parameter_width = 512 * 2;
    stream_audio_texture_parameter4.parameter_height = 1;
    stream_audio_texture_parameter4.data = nullptr;

    AudioRenderStageParameter output_audio_texture_parameter4;
    output_audio_texture_parameter4.name = "output_audio_texture";
    output_audio_texture_parameter4.link_name = nullptr;
    output_audio_texture_parameter4.type = AudioRenderStageParameter::Type::STREAM_OUTPUT;
    output_audio_texture_parameter4.parameter_width = 512 * 2;
    output_audio_texture_parameter4.parameter_height = 1;
    output_audio_texture_parameter4.data = nullptr;

    render_stage5.add_parameter(input_audio_texture_parameter4);
    render_stage5.add_parameter(stream_audio_texture_parameter4);
    render_stage5.add_parameter(output_audio_texture_parameter4);

    render_stage5.m_audio_buffer = audio_buffer4;
    render_stage5.m_fragment_source = R"glsl(
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
            FragColor = vec4(color+color2, color2+color, color2+color, 1.0);
        }
    )glsl";
    audio_renderer.add_render_stage(render_stage5);


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
        REQUIRE(buffer[i] == Catch::Approx(-0.1f - 0.2f - 0.3f - 0.4f + 0.01f + 0.02f + 0.03f + 0.04f + 0.001f + 0.002f + 0.003f + 0.004f));
    }

    t1.detach();
    audio_renderer.main_loop();
}