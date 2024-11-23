#include <GL/glew.h>
#include <GL/freeglut.h>
#include <iostream>

// Shader programs
GLuint shaderProgram1, shaderProgram2;
GLuint uboSharedData;

// UBO binding point
const GLuint UBO_BINDING_POINT = 0;

// Global color and brightness variables
GLfloat globalColor[4] = {1.0f, 0.5f, 0.5f, 1.0f}; // RGBA color
GLfloat brightness = 1.0f;

// Vertex Shader
const char* vertexShaderSource = R"(
    #version 300 es
    layout(location = 0) in vec4 position;
    void main() {
        gl_Position = position;
    }
)";

// Fragment Shader 1: Basic Color
const char* fragmentShaderSource1 = R"(
    #version 300 es
    precision mediump float;
    layout(std140) uniform uSharedData {
        vec4 uGlobalColor;
        float uBrightness;
    };
    out vec4 fragColor;
    void main() {
        fragColor = uGlobalColor * uBrightness;
    }
)";

// Fragment Shader 2: Inverted Color
const char* fragmentShaderSource2 = R"(
    #version 300 es
    precision mediump float;
    layout(std140) uniform uSharedData {
        vec4 uGlobalColor;
        float uBrightness;
    };
    out vec4 fragColor;
    void main() {
        fragColor = vec4((1.0 - uGlobalColor.rgb) * uBrightness, uGlobalColor.a);
    }
)";

// Compile and link shader
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "Error: Shader compilation failed\n" << infoLog << std::endl;
    }
    return shader;
}

GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

void init() {
    glewInit();

    // Create and link shaders
    shaderProgram1 = createShaderProgram(vertexShaderSource, fragmentShaderSource1);
    shaderProgram2 = createShaderProgram(vertexShaderSource, fragmentShaderSource2);

    // Set up Uniform Buffer Object
    glGenBuffers(1, &uboSharedData);
    glBindBuffer(GL_UNIFORM_BUFFER, uboSharedData);

    // Allocate buffer storage
    glBufferData(GL_UNIFORM_BUFFER, sizeof(globalColor) + sizeof(brightness), NULL, GL_STATIC_DRAW);

    // Initialize buffer data
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(globalColor), &globalColor);
    glBufferSubData(GL_UNIFORM_BUFFER, sizeof(globalColor), sizeof(brightness), &brightness);

    // Bind the UBO to the binding point
    glBindBufferBase(GL_UNIFORM_BUFFER, UBO_BINDING_POINT, uboSharedData);

    // Set the uniform block binding for both shaders
    GLuint uboIndex = glGetUniformBlockIndex(shaderProgram1, "uSharedData");
    glUniformBlockBinding(shaderProgram1, uboIndex, UBO_BINDING_POINT);
    uboIndex = glGetUniformBlockIndex(shaderProgram2, "uSharedData");
    glUniformBlockBinding(shaderProgram2, uboIndex, UBO_BINDING_POINT);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Set clear color and enable depth testing
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glEnable(GL_DEPTH_TEST);
}

void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLfloat vertices1[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };

    GLfloat vertices2[] = {
         0.0f, -0.5f, 0.0f,
         1.0f, -0.5f, 0.0f,
         0.5f,  0.5f, 0.0f
    };

    GLuint VBO[2], VAO[2];
    glGenVertexArrays(2, VAO);
    glGenBuffers(2, VBO);

    // Setup first triangle
    glBindVertexArray(VAO[0]);
    glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices1), vertices1, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // Render first triangle with shader 1
    glUseProgram(shaderProgram1);
    glBindVertexArray(VAO[0]);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Setup second triangle
    glBindVertexArray(VAO[1]);
    glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices2), vertices2, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // Render second triangle with shader 2
    glUseProgram(shaderProgram2);
    glBindVertexArray(VAO[1]);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Clean up
    glBindVertexArray(0);
    glutSwapBuffers();
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case '+':
            brightness = std::min(brightness + 0.1f, 2.0f);
            break;
        case '-':
            brightness = std::max(brightness - 0.1f, 0.0f);
            break;
        case 27: // Escape key
            glutLeaveMainLoop();
            break;
    }
    // Update the UBO with the new brightness
    glBindBuffer(GL_UNIFORM_BUFFER, uboSharedData);
    glBufferSubData(GL_UNIFORM_BUFFER, sizeof(globalColor), sizeof(brightness), &brightness);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glutPostRedisplay();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("OpenGL UBO Example");

    init();
    glutDisplayFunc(renderScene);
    glutKeyboardFunc(keyboard);

    glutMainLoop();
    return 0;
}
