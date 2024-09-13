#include "catch2/catch_all.hpp"
#include <GL/glew.h>
#include <GL/glut.h>

#include "audio_texture2d_parameter.h"
#include "audio_render_stage.h"

class TestFixture {
public:
    TestFixture() {
        if (!initialized) {
                // Init the GL context
                int argc = 0;
                char ** argv = nullptr;
                glutInit(&argc, argv);
                glutInitDisplayMode(GLUT_RGBA | GLUT_SINGLE);
                glutCreateWindow("Audio Processing");
                glewInit();

                initialized = true;
        }
    }
    static bool initialized;
};

bool TestFixture::initialized = false;

TEST_CASE_METHOD(TestFixture, "AudioRendererStageTest_get_parameters_with_type") {

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

    REQUIRE(render_stage.add_parameter_old(parameter1));
    REQUIRE(render_stage.add_parameter_old(parameter2));

    render_stage.init_params();

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

    REQUIRE(render_stage2.add_parameter_old(parameter3));
    REQUIRE(render_stage2.add_parameter_old(parameter4));

    render_stage2.init_params();

    // Check the parameters
    auto output_params = render_stage.get_parameters_with_type(AudioRenderStageParameter::Type::STREAM_OUTPUT);
    REQUIRE(output_params.size() == 1);
    REQUIRE(output_params.find("output_parameter") != output_params.end());
    
    auto input_params = render_stage2.get_parameters_with_type(AudioRenderStageParameter::Type::STREAM_INPUT);
    REQUIRE(input_params.size() == 1);
    REQUIRE(input_params.find("input_parameter2") != input_params.end());

    // Link the stages
    REQUIRE(AudioRenderStage::link_stages(render_stage, render_stage2));
}

TEST_CASE_METHOD(TestFixture, "AudioRendererStageTest_add_parameter") {
    AudioRenderStage render_stage = AudioRenderStage(512, 44100, 2);

    auto parameter1 =std::make_unique<AudioTexture2DParameter>(
                                      "input_parameter",
                                      AudioParameter::ConnectionType::INPUT,
                                      512 * 2,
                                      1);

    render_stage.add_parameter(std::move(parameter1));
}

