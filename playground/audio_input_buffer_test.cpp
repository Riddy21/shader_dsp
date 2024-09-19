#include <GL/glew.h>
#include <GL/freeglut.h>
#include <iostream>
#include <vector>
#include <cmath>  // For dynamic motion

GLuint shaderProgram;
GLuint colorUBO, positionUBO;

struct ColorData {
    GLfloat color[4];  // RGBA color
};

struct PositionData {
    GLfloat position[2];  // 2D position
};

std::vector<ColorData> colorData;
std::vector<PositionData> positionData;

float timeValue = 0.0f;  // For dynamic updates

void checkShaderCompilation(GLuint shader) {
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
}

void checkProgramLinking(GLuint program) {
    GLint success;
    GLchar infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cerr << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
}

void createShaderProgram() {
    const char* vertexSource = R"(
        #version 300 es
        layout(location = 0) in vec2 vertexPosition;

        void main() {
            gl_Position = vec4(vertexPosition, 0.0, 1.0);
        }
    )";

    const char* fragmentSource = R"(
        #version 300 es
        precision mediump float;

        layout(std140) uniform ColorBuffer {
            vec4 colors[3];
        };

        layout(std140) uniform PositionBuffer {
            vec2 positions[3];
        };

        uniform int objectIndex;

        out vec4 fragColor;

        void main() {
            fragColor = colors[objectIndex];
        }
    )";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    checkShaderCompilation(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    checkShaderCompilation(fragmentShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    checkProgramLinking(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void initUBOs() {
    // Allocate buffer for color and position data dynamically
    glGenBuffers(1, &colorUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, colorUBO);
    glBufferData(GL_UNIFORM_BUFFER, colorData.size() * sizeof(ColorData), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, colorUBO);

    glGenBuffers(1, &positionUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, positionUBO);
    glBufferData(GL_UNIFORM_BUFFER, positionData.size() * sizeof(PositionData), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, positionUBO);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);  // Unbind
}

void updateUBOData() {
    timeValue += 0.05f;

    for (int i = 0; i < colorData.size(); i++) {
        // Update color dynamically
        colorData[i].color[0] = (sin(timeValue + i) + 1.0f) / 2.0f;  // Red oscillates
        colorData[i].color[1] = (cos(timeValue + i) + 1.0f) / 2.0f;  // Green oscillates
        colorData[i].color[2] = (sin(timeValue + i) * cos(timeValue + i) + 1.0f) / 2.0f;  // Blue oscillates

        // Update position dynamically (circular movement)
        positionData[i].position[0] = cos(timeValue + i) * 0.5f;  // X position
        positionData[i].position[1] = sin(timeValue + i) * 0.5f;  // Y position
    }

    // Update the UBOs with new data
    glBindBuffer(GL_UNIFORM_BUFFER, colorUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, colorData.size() * sizeof(ColorData), colorData.data());

    glBindBuffer(GL_UNIFORM_BUFFER, positionUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, positionData.size() * sizeof(PositionData), positionData.data());

    glBindBuffer(GL_UNIFORM_BUFFER, 0);  // Unbind
}

void renderObject(int objectIndex) {
    GLint objectIndexLocation = glGetUniformLocation(shaderProgram, "objectIndex");
    glUniform1i(objectIndexLocation, objectIndex);

    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shaderProgram);

    updateUBOData();

    for (int i = 0; i < colorData.size(); i++) {
        renderObject(i);  // Render each object using updated data
    }

    glutSwapBuffers();
}

void init() {
    createShaderProgram();

    colorData = {
        {{1.0f, 0.0f, 0.0f, 1.0f}},
        {{0.0f, 1.0f, 0.0f, 1.0f}},
        {{0.0f, 0.0f, 1.0f, 1.0f}}
    };

    positionData = {
        {{0.0f, 0.0f}},
        {{0.5f, 0.5f}},
        {{-0.5f, -0.5f}}
    };

    initUBOs();

    GLfloat vertices[] = {
        -0.1f, -0.1f,
         0.1f, -0.1f,
         0.0f,  0.1f
    };

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Dynamic UBO Example");

    glewExperimental = GL_TRUE;
    glewInit();

    init();

    glutDisplayFunc(display);
    glutIdleFunc(display);  // Continuously render

    glutMainLoop();
    return 0;
}
