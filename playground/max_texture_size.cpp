#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <iostream>

void printMaxTextureSize() {
    GLint maxTextureSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    std::cout << "Maximum texture size: " << maxTextureSize << "x" << maxTextureSize << std::endl;
}

int main(int argc, char** argv) {
    // Initialize OpenGL context here...
    glutInit(&argc, argv);
    glewInit();

    // Print the maximum texture size
    printMaxTextureSize();

    return 0;
}