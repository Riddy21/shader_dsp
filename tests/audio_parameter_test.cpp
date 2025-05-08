#include "catch2/catch_all.hpp"
#include <vector>

#include "audio_core/audio_renderer.h"
#include "audio_core/audio_render_graph.h"
#include "audio_core/audio_parameter.h"
#include "audio_output/audio_player_output.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"
#include "audio_parameter/audio_texture2d_parameter.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_parameter/audio_uniform_array_parameter.h"

TEST_CASE("MakeUniqueTest") {
    std::vector<std::unique_ptr<AudioParameter>> audio_parameters;

    auto audio_parameter = std::make_unique<AudioTexture2DParameter>("audio_parameter",
                                                                      AudioParameter::ConnectionType::INPUT,
                                                                      512,
                                                                      512);

    audio_parameters.push_back(std::move(audio_parameter));

    REQUIRE(audio_parameters.size() == 1);
    float * value = new float[512*512]();
    REQUIRE(audio_parameters[0]->set_value(value));
    delete[] value;

    // Cast to a 2D parameter
    auto audio_texture = dynamic_cast<AudioTexture2DParameter *>(audio_parameters[0].get())->name;
    REQUIRE(audio_texture == "audio_parameter");

    auto time_parameter = std::make_unique<AudioIntBufferParameter>("time",
                                                              AudioParameter::ConnectionType::INPUT);
    time_parameter->set_value(19);

    audio_parameters.push_back(std::move(time_parameter));
}

TEST_CASE("MakeArrayParameterTest") {

    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    auto audio_final_render_stage = new AudioFinalRenderStage(512, 44100, 2);

    auto render_stage = new AudioRenderStage(512, 44100, 2,
                                             "build/shaders/array_parameter_test.glsl");

    auto input_array = new AudioIntArrayParameter("input_array",
                                                  AudioParameter::ConnectionType::INPUT,
                                                  512);

    int* array_values = new int[512];
    for (int i = 0; i < 512; ++i) {
        array_values[i] = i + 1;
    }
    input_array->set_value(array_values);

    auto output_texture = new AudioTexture2DParameter("output_debug_texture",
                                                      AudioParameter::ConnectionType::OUTPUT,
                                                      512, 2,
                                                      0,
                                                      2,
                                                      GL_NEAREST);

    render_stage->add_parameter(input_array);
    render_stage->add_parameter(output_texture);

    render_stage->connect_render_stage(audio_final_render_stage);

    auto audio_driver = new AudioPlayerOutput(512, 44100, 2);
    
    auto audio_render_graph = new AudioRenderGraph({audio_final_render_stage});

    audio_renderer.add_render_graph(audio_render_graph);
    
    audio_renderer.add_render_output(audio_driver);

    REQUIRE(audio_renderer.initialize(512, 44100, 2));

    REQUIRE(audio_driver->open());
    REQUIRE(audio_driver->start());

    audio_renderer.increment_main_loop();
    float * debug_output = (float *)render_stage->find_parameter("output_debug_texture")->get_value();

    for (int i = 0; i < 512; ++i) {
        REQUIRE(debug_output[i] == i + 1);
        // check the type if its a float or an int
        REQUIRE(debug_output[i] == static_cast<float>(i + 1));
    }

    delete [] array_values;

    REQUIRE(audio_renderer.terminate());

}

TEST_CASE("MakeFloatArrayParameterTest") {

    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    auto audio_final_render_stage = new AudioFinalRenderStage(512, 44100, 2);

    auto render_stage = new AudioRenderStage(512, 44100, 2,
                                             "build/shaders/float_array_parameter_test.glsl");

    auto input_array = new AudioFloatArrayParameter("input_float_array",
                                                    AudioParameter::ConnectionType::INPUT,
                                                    512);

    float* array_values = new float[512];
    for (int i = 0; i < 512; ++i) {
        array_values[i] = static_cast<float>(i) + 0.5f;
    }
    input_array->set_value(array_values);

    auto output_texture = new AudioTexture2DParameter("output_debug_texture",
                                                      AudioParameter::ConnectionType::OUTPUT,
                                                      512, 2,
                                                      0,
                                                      2,
                                                      GL_NEAREST);

    render_stage->add_parameter(input_array);
    render_stage->add_parameter(output_texture);

    render_stage->connect_render_stage(audio_final_render_stage);

    auto audio_driver = new AudioPlayerOutput(512, 44100, 2);
    
    auto audio_render_graph = new AudioRenderGraph({audio_final_render_stage});

    audio_renderer.add_render_graph(audio_render_graph);
    
    audio_renderer.add_render_output(audio_driver);

    REQUIRE(audio_renderer.initialize(512, 44100, 2));

    REQUIRE(audio_driver->open());
    REQUIRE(audio_driver->start());

    audio_renderer.increment_main_loop();
    float * debug_output = (float *)render_stage->find_parameter("output_debug_texture")->get_value();

    for (int i = 0; i < 512; ++i) {
        REQUIRE(debug_output[i] == static_cast<float>(i) + 0.5f);
    }

    delete[] array_values;

    REQUIRE(audio_renderer.terminate());
}

TEST_CASE("MakeBoolArrayParameterTest") {

    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    auto audio_final_render_stage = new AudioFinalRenderStage(512, 44100, 2);

    auto render_stage = new AudioRenderStage(512, 44100, 2,
                                             "build/shaders/bool_array_parameter_test.glsl");

    auto input_array = new AudioBoolArrayParameter("input_bool_array",
                                                    AudioParameter::ConnectionType::INPUT,
                                                    512);

    bool * array_values = new bool[512];
    for (int i = 0; i < 512; ++i) {
        array_values[i] = i % 2 == 0;
    }
    input_array->set_value(array_values);

    auto output_texture = new AudioTexture2DParameter("output_debug_texture",
                                                      AudioParameter::ConnectionType::OUTPUT,
                                                      512, 2,
                                                      0,
                                                      2,
                                                      GL_NEAREST);

    render_stage->add_parameter(input_array);
    render_stage->add_parameter(output_texture);

    render_stage->connect_render_stage(audio_final_render_stage);

    auto audio_driver = new AudioPlayerOutput(512, 44100, 2);
    
    auto audio_render_graph = new AudioRenderGraph({audio_final_render_stage});

    audio_renderer.add_render_graph(audio_render_graph);
    
    audio_renderer.add_render_output(audio_driver);

    REQUIRE(audio_renderer.initialize(512, 44100, 2));

    REQUIRE(audio_driver->open());
    REQUIRE(audio_driver->start());

    audio_renderer.increment_main_loop();
    float * debug_output = (float *)render_stage->find_parameter("output_debug_texture")->get_value();

    for (int i = 0; i < 512; ++i) {
        REQUIRE(debug_output[i] == static_cast<float>(array_values[i]));
    }


    delete[] array_values;

    REQUIRE(audio_renderer.terminate());
}