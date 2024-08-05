#include <GL/glew.h>
#include <GL/glut.h>
#include <iostream>

#include "audio_renderer.h"

AudioRenderer::AudioRenderer(const unsigned int buffer_size, const unsigned int num_channels) : 
    buffer_size(buffer_size),
    num_channels(num_channels)
{
}

AudioRenderer::~AudioRenderer()
{
}

bool AudioRenderer::init()
{
    int argc = 0;
    char** argv = nullptr;
    glutInit(&argc, argv);

    // Init the OpenGL context
    glutInitDisplayMode(GLUT_RGBA | GLUT_SINGLE);
    glutInitWindowSize(buffer_size, num_channels);
    glutCreateWindow("Audio Processing");

    // Initialize GLEW
    GLenum glewInitResult = glewInit();
    if (glewInitResult != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(glewInitResult) << std::endl;
        return false;
    }

    // Print the OpenGL version
    const GLubyte* glVersion = glGetString(GL_VERSION);
    const GLubyte* glslVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);
    
    std::cout << "OpenGL Version: " << glVersion << std::endl;
    std::cout << "GLSL Version: " << glslVersion << std::endl;

    // Set GL settings for audio rendering
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);

    // Generate Buffer Objects
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glGenFramebuffers(2, FBO);
    glGenTextures(2, audio_texture);

    return true;
}

bool AudioRenderer::cleanup()
{
    // TODO: Add cleanup code here for anything that needs to be cleaned up
    return true;
}

bool AudioRenderer::terminate()
{
    // Clean up the OpenGL context
    if (!cleanup()) {
        return false;
    }

    // Terminate the OpenGL context
    glutDestroyWindow(glutGetWindow());

    return true;
}