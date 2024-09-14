#include <GL/glew.h>
#include <GL/glut.h>
#include <iostream>
#include <vector>

const int BUFFER_SIZE = 512;

GLuint texture1;
GLuint texture2;
GLuint shaderProgram;
GLuint PBO_in;
GLuint PBO_out;
GLuint FBO;
GLuint VAO;
GLuint VBO;

const GLchar * vertexShaderSource = R"glsl(
    #version 150 core
    in vec2 position;
    in float texCoord;
    out float TexCoord;
    void main()
    {
        gl_Position = vec4(position, 0.0, 1.0);
        TexCoord = texCoord;
    }
)glsl";

const GLchar * fragmentShaderSource = R"glsl(
    #version 150 core
    in float TexCoord;
    uniform sampler2D audioTexture1;
    uniform sampler2D audioTexture2;
    out vec4 FragColor;
    void main ()
    {
        float color1 = texture(audioTexture1, vec2(TexCoord, 0.5)).r;
        float color2 = texture(audioTexture2, vec2(TexCoord, 0.5)).r;
        FragColor = vec4(color1, color2, color2, 1.0);
    }
)glsl";

GLuint createShaderProgram(const GLchar * vertexShaderSource, const GLchar * fragmentShaderSource)
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    return shaderProgram;
}

void display()
{
    // update the pixel buffer
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO_in);
    float *ptr = (float * )glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    if (ptr) {
        for (int i = 0; i < BUFFER_SIZE; i++) {
            ptr[i] = (float) i / BUFFER_SIZE;
        }
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    }

    // Update the textures
    glBindTexture(GL_TEXTURE_2D, texture1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, BUFFER_SIZE, 1, GL_RED, GL_FLOAT, nullptr);

    // update the pixel buffer
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO_in);
    float *ptr2 = (float * )glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    if (ptr2) {
        for (int i = 0; i < BUFFER_SIZE; i++) {
            ptr2[i] = (float) (BUFFER_SIZE - i) / BUFFER_SIZE;
        }
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    }

    glBindTexture(GL_TEXTURE_2D, texture2);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, BUFFER_SIZE, 1, GL_RED, GL_FLOAT, nullptr);

    // Render the texture to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Read the framebuffer
    glBindBuffer(GL_PIXEL_PACK_BUFFER, PBO_out);
    glReadPixels(0, 0, BUFFER_SIZE, 1, GL_RED, GL_FLOAT, 0);
    float * ptr3 = (float *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    if (ptr3) {
        for (int i = 0; i < BUFFER_SIZE; i++) {
            std::cout << ptr3[i] << " ";
        }
        std::cout << std::endl;
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Unbind everything
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Swap buffers
    glutSwapBuffers();
    glutPostRedisplay();

}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_SINGLE);
    glutInitWindowSize(512, 256);
    glutCreateWindow("Audio Generator Test");

    glewInit();

    GLfloat vertices[] = {
        -1.0f, -1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,
        1.0f,  -1.0f, 1.0f,
        1.0f,  1.0f, 1.0f,
        -1.0f, 1.0f, 0.0f,
        1.0f,  -1.0f, 1.0f
    }; // Spans the entire screen with the texture coordinates

    shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    // Setup textures and framebuffer
    glGenTextures(1, &texture1);
    glGenTextures(1, &texture2);
    glGenFramebuffers(1, &FBO);
    glBindTexture(GL_TEXTURE_2D, texture1);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    float flatColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, flatColor);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture1, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, BUFFER_SIZE, 1, 0, GL_RED, GL_FLOAT, nullptr);

    glBindTexture(GL_TEXTURE_2D, texture2);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, flatColor);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture2, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, BUFFER_SIZE, 1, 0, GL_RED, GL_FLOAT, nullptr);

    // setup PBO
    glGenBuffers(1, &PBO_in);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO_in);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, BUFFER_SIZE * sizeof(float), nullptr, GL_STREAM_DRAW);

    glGenBuffers(1, &PBO_out);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, PBO_out);
    glBufferData(GL_PIXEL_PACK_BUFFER, BUFFER_SIZE * sizeof(float), nullptr, GL_STREAM_READ);

    // Setup VAO and VBO
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid *)(2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    // Unbind everything
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    display();
    glutMainLoop();
    return 0;
}