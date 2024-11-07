#include <iostream>
#include <GL/glew.h>
#include <GL/glut.h>

void checkFramebufferLimits() {
    GLint maxColorAttachments;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);
    std::cout << "Maximum number of color attachments: " << maxColorAttachments << std::endl;

    GLint maxDrawBuffers;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    std::cout << "Maximum number of draw buffers: " << maxDrawBuffers << std::endl;
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glewInit();

    checkFramebufferLimits();

    return 0;
}