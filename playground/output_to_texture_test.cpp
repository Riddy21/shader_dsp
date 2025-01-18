#include <GL/glew.h>
#include <GL/freeglut.h>
#include <iostream>
#include <vector>

// Simple vertex/fragment shaders that output to two color attachments
static const char* vertShader = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

static const char* fragShader = R"(
#version 330 core
out vec4 output0; // layout(location=0) by default
layout(location = 1) out vec4 output1;

void main() {
    float gray = (gl_FragCoord.x / 512.0);
    // A gradient in the first output
    output0 = vec4(gray, gray, gray, 1.0);
    // A red color in the second output
    output1 = vec4(1.0, 0.0, 0.0, 1.0);
}
)";

GLuint program = 0;
GLuint fbo = 0;
GLuint tex0 = 0, tex1 = 0;
GLuint VAO = 0, VBO = 0;

// Helper to check for shader compilation and linking errors (simplified)
bool checkShaderStatus(GLuint shader) {
    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "Shader compile error: " << log << std::endl;
        return false;
    }
    return true;
}

bool checkProgramStatus(GLuint prog) {
    GLint status = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (!status) {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        std::cerr << "Program link error: " << log << std::endl;
        return false;
    }
    return true;
}

void displayCallback() {
    // 1. Render to the FBO
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 512, 512);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(program);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // 2. Read back two textures with glGetTexImage
    std::vector<unsigned char> data0(512 * 512 * 4);
    std::vector<unsigned char> data1(512 * 512 * 4);

    glBindTexture(GL_TEXTURE_2D, tex0);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data0.data());

    glBindTexture(GL_TEXTURE_2D, tex1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data1.data());

    // Just print a small portion to show difference
    std::cout << "Texture0 first 4 RGBA: ";
    for (int i = 0; i < 8; ++i) std::cout << (int)data0[i] << " ";
    std::cout << "\nTexture1 first 4 RGBA: ";
    for (int i = 0; i < 8; ++i) std::cout << (int)data1[i] << " ";
    std::cout << std::endl;

    // Cleanup and exit
    glutLeaveMainLoop();
}

int main(int argc, char** argv) {
    // 1. Initialize freeglut
    glutInit(&argc, argv);
    glutInitContextVersion(3, 3);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(512, 512);
    glutCreateWindow("FBO Multi-Output Example (freeglut)");

    // 2. Initialize GLEW
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: "
                  << glewGetErrorString(err) << std::endl;
        return -1;
    }

    // 3. Build the shader program
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertShader, nullptr);
    glCompileShader(vs);
    if (!checkShaderStatus(vs)) return -1;

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragShader, nullptr);
    glCompileShader(fs);
    if (!checkShaderStatus(fs)) return -1;

    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    if (!checkProgramStatus(program)) return -1;

    glDeleteShader(vs);
    glDeleteShader(fs);

    // 4. Create a simple quad with VAO/VBO
    float quadVertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);

    // 5. Create and bind the FBO
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // 6. Create two textures, attach each to a different color attachment
    glGenTextures(1, &tex0);
    glBindTexture(GL_TEXTURE_2D, tex0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 1, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, tex0, 0);

    glGenTextures(1, &tex1);
    glBindTexture(GL_TEXTURE_2D, tex1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 1, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                           GL_TEXTURE_2D, tex1, 0);

    GLenum buffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, buffers);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer incomplete!" << std::endl;
        return -1;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 7. Register display callback and enter main loop
    glutDisplayFunc(displayCallback);
    glutMainLoop();

    // 8. Cleanup after we exit main loop
    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
    glDeleteProgram(program);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex0);
    glDeleteTextures(1, &tex1);

    return 0;
}