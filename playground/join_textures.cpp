#include <GL/glew.h>
#include <GL/freeglut.h>
#include <iostream>
#include <vector>

// Shader sources
const GLchar* vertexSource =
    "#version 150 core\n"
    "in vec2 position;"
    "in vec2 texcoord;"
    "out vec2 Texcoord;"
    "void main() {"
    "   Texcoord = texcoord;"
    "   gl_Position = vec4(position, 0.0, 1.0);"
    "}";
const GLchar* fragmentSource =
    "#version 150 core\n"
    "uniform sampler2D texKitten;"
    "uniform sampler2D texPuppy;"
    "in vec2 Texcoord;"
    "out vec4 outColor;"
    "void main() {"
    "   outColor = mix(texture(texKitten, Texcoord), texture(texPuppy, Texcoord), 0.5);"
    "}";

int activeTexture[2] = {0, 0}; // 0 for the first texture, 1 for the second

// Function prototypes
void init();
void display();
void loadTextures();
GLuint createShader(const GLchar* source, GLenum type);
GLuint createTexture(GLsizei width, GLsizei height, bool firstTexture);

// Globals
GLuint vao, vbo, ebo;
GLuint shaderProgram;
GLuint textures[2];

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 'a': // Switch to texture 1
            activeTexture[0] = 1;
            break;
        case 'd': // Switch to texture 2
            activeTexture[1] = 1;
            break;
        default:
            activeTexture[0] = 0;
            activeTexture[1] = 0;
            return; // If any other key is pressed, do nothing
    }
    glutPostRedisplay(); // Marks the current window as needing to be redisplayed
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitContextVersion(4, 1);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(800, 600);
    glutCreateWindow("OpenGL");

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW." << std::endl;
        return -1;
    }

    init();
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutMainLoop();

    return 0;
}

void init() {
    // Create and compile shaders
    GLuint vertexShader = createShader(vertexSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = createShader(fragmentSource, GL_FRAGMENT_SHADER);
    // Link the vertex and fragment shader into a shader program
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glBindFragDataLocation(shaderProgram, 0, "outColor");
    glLinkProgram(shaderProgram);
    glUseProgram(shaderProgram);

    // Specify the layout of the vertex data
    GLfloat vertices[] = {
        // Position    Texcoords
        -0.5f,  0.5f, 0.0f, 0.0f, // Top-left
         0.5f,  0.5f, 1.0f, 0.0f, // Top-right
         0.5f, -0.5f, 1.0f, 1.0f, // Bottom-right
        -0.5f, -0.5f, 0.0f, 1.0f  // Bottom-left
    };
    GLuint elements[] = {
        0, 1, 2,
        2, 3, 0
    };

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);

    GLint posAttrib = glGetAttribLocation(shaderProgram, "position");
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
    glEnableVertexAttribArray(posAttrib);

    GLint texAttrib = glGetAttribLocation(shaderProgram, "texcoord");
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(texAttrib);

    loadTextures();
}

std::vector<unsigned char> genTextureData(GLsizei width, GLsizei height, bool firstTexture){
    std::vector<unsigned char> data(width * height * 3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t index = (y * width + x) * 3;
            data[index] = (unsigned char)(x / (float)width * 255);
            data[index + 1] = (unsigned char)(y / (float)height * 255);
            data[index + 2] = firstTexture ? 0 : 255; // Differentiate the two textures
        }
    }
    return data;
}

void display() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArray(vao);

    // Update texture data
    unsigned width = 256;
    unsigned height = 256;
    std::vector<unsigned char> data(width * height * 3);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glUniform1i(glGetUniformLocation(shaderProgram, "texKitten"), 0);
    // Update texture data
    if (activeTexture[0])
        data = genTextureData(width, height, true);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, &data[0]);


    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glUniform1i(glGetUniformLocation(shaderProgram, "texPuppy"), 1);
    // Update texture data
    if (activeTexture[1])
        data = genTextureData(width, height, false);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, &data[0]);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glutSwapBuffers();
}

GLuint createShader(const GLchar* source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    return shader;
}

void loadTextures() {
    // Generate gradient textures
    textures[0] = createTexture(256, 256, false);
    textures[1] = createTexture(256, 256, true);
}

GLuint createTexture(GLsizei width, GLsizei height, bool firstTexture) {
    std::vector<unsigned char> data(width * height * 3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t index = (y * width + x) * 3;
            data[index] = (unsigned char)(x / (float)width * 255);
            data[index + 1] = (unsigned char)(y / (float)height * 255);
            data[index + 2] = firstTexture ? 0 : 255; // Differentiate the two textures
        }
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, &data[0]);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return texture;
}
