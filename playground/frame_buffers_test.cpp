#include <GL/glew.h>
#include <GL/glut.h>
#include <iostream>

// Shader source code
const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(position, 1.0);
    TexCoord = texCoord;
}
)";

const char* fragmentShaderSourceFirstPass = R"(
#version 330 core
out vec4 FragColor1;
out vec4 FragColor2;

in vec2 TexCoord;
uniform sampler2D texture1;

void main() {
    vec4 data = texture(texture1, TexCoord);
    FragColor1 = data + vec4(0.0, 1.0, 0.1, 1.0);  // green tint
    FragColor2 = vec4(0.0, TexCoord, 1.0);  // red tint
}
)";

const char* fragmentShaderSourceSecondPass = R"(
#version 330 core
out vec4 FragColor;

uniform sampler2D output1;
uniform sampler2D output2;

in vec2 TexCoord;

void main() {
    vec4 color1 = texture(output1, TexCoord);
    vec4 color2 = texture(output2, TexCoord);
    FragColor = color1 + color2;  // Combine the two textures
}
)";

// Global variables
GLuint firstPassProgram, secondPassProgram;
GLuint fbo1, tex1, tex2, tex3;
GLuint quadVAO, quadVBO;

// Function to compile shaders
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
        exit(1);
    }
    return shader;
}

// Function to link shaders into a program
GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        exit(1);
    }
    return program;
}

// Function to create shaders and programs
void createShaders() {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);

    GLuint fragmentShaderFirstPass = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSourceFirstPass);
    firstPassProgram = linkProgram(vertexShader, fragmentShaderFirstPass);

    GLuint fragmentShaderSecondPass = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSourceSecondPass);
    secondPassProgram = linkProgram(vertexShader, fragmentShaderSecondPass);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShaderFirstPass);
    glDeleteShader(fragmentShaderSecondPass);
}

// Function to create framebuffers and textures
void createFramebuffers() {
    glGenFramebuffers(1, &fbo1);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo1);

    glGenTextures(1, &tex1);
    glBindTexture(GL_TEXTURE_2D, tex1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 800, 600, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);

    glGenTextures(1, &tex2);
    glBindTexture(GL_TEXTURE_2D, tex2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 800, 600, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, tex2, 0);

    GLenum drawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, drawBuffers);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Error creating framebuffer!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Generate another texture for inputting to first one
    glGenTextures(1, &tex3);
    glBindTexture(GL_TEXTURE_2D, tex3);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 800, 600, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    char data[800 * 600 * 4];
    for (int i = 0; i < 800 * 600 * 4; i += 4) {
        data[i] = 255;
        data[i + 1] = 0;
        data[i + 2] = 0;
        data[i + 3] = 255;
    }
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 800, 600, GL_RGBA, GL_UNSIGNED_BYTE, data);
}

// Function to create a fullscreen quad
void createQuad() {
    float quadVertices[] = {
        // Positions     // Texture Coords
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
         1.0f, -1.0f, 0.0f,  1.0f, 0.0f,

        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
         1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);

    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
}

// Render to textures in the first pass
void renderToTextures() {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(firstPassProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex3);
    glUniform1i(glGetUniformLocation(firstPassProgram, "texture1"), 0);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Render to screen in the second pass
void renderToScreen() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(secondPassProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex1);
    glUniform1i(glGetUniformLocation(secondPassProgram, "output1"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex2);
    glUniform1i(glGetUniformLocation(secondPassProgram, "output2"), 1);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

// Display callback function
void display() {
    renderToTextures();
    renderToScreen();
    glutSwapBuffers();
}

// Initialize OpenGL settings
void initGL() {
    glewExperimental = GL_TRUE;
    GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK) {
        std::cerr << "Error: " << glewGetErrorString(glewStatus) << std::endl;
        exit(1);
    }

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    createShaders();
    createFramebuffers();
    createQuad();
}

// Main function
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Multi-pass Shader Example");

    initGL();

    glutDisplayFunc(display);
    glutMainLoop();
    return 0;
}
