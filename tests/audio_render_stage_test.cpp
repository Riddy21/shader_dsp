#include "catch2/catch_all.hpp"
#include <GL/glew.h>
#include <GL/glut.h>

#include "audio_render_stage.h"

TEST_CASE("AudioRendererStageTest_get_parameters_with_type") {
    // Init the GL context
    int argc = 0;
    char ** argv = nullptr;
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_SINGLE);
    glutCreateWindow("Audio Processing");
    glewInit();

    // placeholder frame buffer
    GLuint FBO;
    glGenFramebuffers(1, &FBO);

    AudioRenderStage render_stage = AudioRenderStage(512, 44100, 2);

    const float * empty_data = new float[512 * 2]();

    AudioRenderStageParameter parameter1;
    parameter1.name = "input_parameter";
    parameter1.type = AudioRenderStageParameter::Type::STREAM_INPUT;
    parameter1.datatype = AudioRenderStageParameter::DataType::FLOAT;
    parameter1.format = AudioRenderStageParameter::Format::RED;
    parameter1.internal_format = AudioRenderStageParameter::InternalFormat::R32F;
    parameter1.parameter_width = 512 * 2;
    parameter1.parameter_height = 1;
    parameter1.data = &empty_data;
    parameter1.texture = AudioRenderStageParameter::generate_texture(parameter1);

    AudioRenderStageParameter parameter2;
    parameter2.name = "output_parameter";
    parameter2.link_name = "input_parameter2";
    parameter2.type = AudioRenderStageParameter::Type::STREAM_OUTPUT;
    parameter2.datatype = AudioRenderStageParameter::DataType::FLOAT;
    parameter2.format = AudioRenderStageParameter::Format::RED;
    parameter2.internal_format = AudioRenderStageParameter::InternalFormat::R32F;
    parameter2.parameter_width = 512 * 2;
    parameter2.parameter_height = 1;
    parameter2.data = nullptr;
    parameter2.framebuffer = FBO;

    REQUIRE(render_stage.add_parameter(parameter1));
    REQUIRE(render_stage.add_parameter(parameter2));

    AudioRenderStage render_stage2 = AudioRenderStage(512, 44100, 2);

    AudioRenderStageParameter parameter3;
    parameter3.name = "input_parameter2";
    parameter3.link_name = "output_parameter";
    parameter3.type = AudioRenderStageParameter::Type::STREAM_INPUT;
    parameter3.datatype = AudioRenderStageParameter::DataType::FLOAT;
    parameter3.format = AudioRenderStageParameter::Format::RED;
    parameter3.internal_format = AudioRenderStageParameter::InternalFormat::R32F;
    parameter3.parameter_width = 512 * 2;
    parameter3.parameter_height = 1;
    parameter3.data = nullptr;
    parameter3.texture = AudioRenderStageParameter::generate_texture(parameter3);

    AudioRenderStageParameter parameter4;
    parameter4.name = "output_parameter";
    parameter4.type = AudioRenderStageParameter::Type::STREAM_OUTPUT;
    parameter4.datatype = AudioRenderStageParameter::DataType::FLOAT;
    parameter4.format = AudioRenderStageParameter::Format::RED;
    parameter4.internal_format = AudioRenderStageParameter::InternalFormat::R32F;
    parameter4.parameter_width = 512 * 2;
    parameter4.parameter_height = 1;
    parameter4.data = nullptr;
    parameter4.framebuffer = FBO;

    REQUIRE(render_stage2.add_parameter(parameter3));
    REQUIRE(render_stage2.add_parameter(parameter4));

    // Check the parameters
    auto output_params = render_stage.get_parameters_with_type(AudioRenderStageParameter::Type::STREAM_OUTPUT);
    REQUIRE(output_params.size() == 1);
    REQUIRE(output_params.find("output_parameter") != output_params.end());
    
    auto input_params = render_stage2.get_parameters_with_type(AudioRenderStageParameter::Type::STREAM_INPUT);
    REQUIRE(input_params.size() == 1);
    REQUIRE(input_params.find("input_parameter2") != input_params.end());

    // Link the stages
    bool linked = AudioRenderStage::link_stages(render_stage, render_stage2);
    REQUIRE(linked == true);
}