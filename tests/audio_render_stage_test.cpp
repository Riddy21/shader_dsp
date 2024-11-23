#include "catch2/catch_all.hpp"
#include <GL/glew.h>
#include <GL/glut.h>

#include "audio_parameter/audio_texture2d_parameter.h"
#include "audio_render_stage/audio_render_stage.h"

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

TEST_CASE_METHOD(TestFixture, "AudioRendererStageTest_add_parameter") {
    AudioRenderStage render_stage = AudioRenderStage(512, 44100, 2);

    auto parameter1 = new AudioTexture2DParameter(
                                      "input_parameter",
                                      AudioParameter::ConnectionType::INPUT,
                                      512 * 2,
                                      1);

    render_stage.add_parameter(parameter1);
}