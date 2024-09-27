#include <GL/glew.h>
#include <GL/glut.h>
#include <iostream>
#include <cmath>

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
uniform float u_time;

void main() {
    vec4 data = texture(texture1, TexCoord);
    FragColor1 = data + vec4(0.0, 1.0, 0.1 * sin(u_time), 1.0);  // green tint with time-based modulation
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

// Function to update the uniform float parameter
void updateUniformFloat(GLuint program, const char* uniformName, float value) {
    glUseProgram(program);
    GLint location = glGetUniformLocation(program, uniformName);
    if (location == -1) {
        std::cerr << "ERROR::UNIFORM::NOT_FOUND " << uniformName << std::endl;
        return;
    }
    glUniform1f(location, value);
}

int main(int argc, char** argv) {
    // Initialize GLUT and create a window
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(800, 600);
    glutCreateWindow("OpenGL Example");

    // Initialize GLEW
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "ERROR::GLEW::INIT_FAILED " << glewGetErrorString(err) << std::endl;
        return -1;
    }

    // Compile and link shaders
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShaderFirstPass = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSourceFirstPass);
    firstPassProgram = linkProgram(vertexShader, fragmentShaderFirstPass);

    // Main loop
    while (true) {
        // Update the uniform float parameter
        float timeValue = static_cast<float>(glutGet(GLUT_ELAPSED_TIME)) / 1000.0f;
        updateUniformFloat(firstPassProgram, "u_time", timeValue);

        // Render your scene here...

        // Swap buffers
        glutSwapBuffers();
    }

    return 0;
}