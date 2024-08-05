#include "catch2/catch_all.hpp"
#include <GL/glew.h>
#include <GL/glut.h>

#include "audio_generator.h"

TEST_CASE("AudioGenerator") {
    // Intialize the OpenGL context
    // mock the argc and argv
    int argc = 0;
    char** argv = nullptr;
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_SINGLE);
    glutInitWindowSize(512, 512);
    glutCreateWindow("Audio Processing");
    glewInit();

    AudioGenerator audio_generator(512, 44100);
}