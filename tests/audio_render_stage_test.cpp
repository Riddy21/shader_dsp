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

    AudioRenderStageParameter parameter1 =
            AudioRenderStageParameter(
                                      "input_parameter",
                                      AudioRenderStageParameter::Type::STREAM_INPUT,
                                      512 * 2,
                                      1,
                                      &empty_data);

    AudioRenderStageParameter parameter2 = 
            AudioRenderStageParameter("output_parameter",
                                      AudioRenderStageParameter::Type::STREAM_OUTPUT,
                                      512 * 2,
                                      1,
                                      nullptr,
                                      "input_parameter2");

    REQUIRE(render_stage.add_parameter(parameter1));
    REQUIRE(render_stage.add_parameter(parameter2));

    render_stage.init();

    AudioRenderStage render_stage2 = AudioRenderStage(512, 44100, 2);

    AudioRenderStageParameter parameter3 =
            AudioRenderStageParameter(
                                      "input_parameter2",
                                      AudioRenderStageParameter::Type::STREAM_INPUT,
                                      512 * 2,
                                      1,
                                      nullptr,
                                      "output_parameter");

    AudioRenderStageParameter parameter4 =
            AudioRenderStageParameter("output_parameter",
                                      AudioRenderStageParameter::Type::STREAM_OUTPUT,
                                      512 * 2,
                                      1,
                                      nullptr);

    REQUIRE(render_stage2.add_parameter(parameter3));
    REQUIRE(render_stage2.add_parameter(parameter4));

    render_stage2.init();

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