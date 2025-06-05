#include "SDL2/SDL.h"
#include <GL/glew.h>

// --- Setup OpenGL context manually ---
SDL_Window* window = SDL_CreateWindow("GL Test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 512, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
SDL_GLContext gl_context = SDL_GL_CreateContext(window);

// --- Create and compile shader program ---
const char* vert_src =
"#version 330 core\n"
"layout(location = 0) in vec2 aPos;\n"
"layout(location = 1) in vec2 aTexCoord;\n"
"out vec2 TexCoord;\n"
"void main() {\n"
"    gl_Position = vec4(aPos, 0.0, 1.0);\n"
"    TexCoord = aTexCoord;\n"
"}\n";

const char* frag_src =
"#version 330 core\n"
"in vec2 TexCoord;\n"
"out vec4 outputColor;\n"
"void main() {\n"
"    float sineValue = sin(TexCoord.x * 25.6); // 0..1 mapped to 0..256*0.1\n"
"    outputColor = vec4(0.3, 0.0, 0.0, 1.0);\n"
"}\n";

GLuint vert_shader = glCreateShader(GL_VERTEX_SHADER);
glShaderSource(vert_shader, 1, &vert_src, nullptr);
glCompileShader(vert_shader);

GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
glShaderSource(frag_shader, 1, &frag_src, nullptr);
glCompileShader(frag_shader);

GLuint program = glCreateProgram();
glAttachShader(program, vert_shader);
glAttachShader(program, frag_shader);
glLinkProgram(program);

glDeleteShader(vert_shader);
glDeleteShader(frag_shader);

// --- Create framebuffer and output texture ---
const GLuint width = 256;
const GLuint height = 1;
GLuint framebuffer = 0;
glGenFramebuffers(1, &framebuffer);
glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

// Create output texture (to store shader output)
GLuint output_texture = 0;
glGenTextures(1, &output_texture);
glBindTexture(GL_TEXTURE_2D, output_texture);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);

// Attach output texture to framebuffer
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_texture, 0);
GLenum draw_buf = GL_COLOR_ATTACHMENT0;
glDrawBuffers(1, &draw_buf);

REQUIRE(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

// --- Create and bind VAO/VBO for fullscreen quad ---
GLuint vao = 0, vbo = 0;
float quad[] = {
    // aPos.x, aPos.y, aTexCoord.x, aTexCoord.y
    -1, -1, 0.0f, 0.0f,
    -1,  1, 0.0f, 1.0f,
     1, -1, 1.0f, 0.0f,
     1, -1, 1.0f, 0.0f,
    -1,  1, 0.0f, 1.0f,
     1,  1, 1.0f, 1.0f
};
glGenVertexArrays(1, &vao);
glGenBuffers(1, &vbo);
glBindVertexArray(vao);
glBindBuffer(GL_ARRAY_BUFFER, vbo);
glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
glEnableVertexAttribArray(0);
glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
glEnableVertexAttribArray(1);
glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

// --- Render to texture ---
glViewport(0, 0, width, height);
glUseProgram(program);
glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
glClear(GL_COLOR_BUFFER_BIT);
glBindVertexArray(vao);
glDrawArrays(GL_TRIANGLES, 0, 6);
glBindVertexArray(0);
glBindFramebuffer(GL_FRAMEBUFFER, 0);
glUseProgram(0);

// --- Read back texture data from output_texture ---
std::vector<float> pixels(width * 4, 0.0f); // RGBA
glBindTexture(GL_TEXTURE_2D, output_texture);
glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());
glBindTexture(GL_TEXTURE_2D, 0);

// --- Check red channel for sine wave ---
for (size_t i = 0; i < width; ++i) {
    //float expected = sin(static_cast<float>(i) * 0.1f);
    //REQUIRE(pixels[i * 4 + 0] == Catch::Approx(expected));
    printf("Pixel %zu: R = %f, G = %f, B = %f, A = %f\n", i, pixels[i * 4 + 0], pixels[i * 4 + 1], pixels[i * 4 + 2], pixels[i * 4 + 3]);
}

// --- Cleanup ---
glDeleteBuffers(1, &vbo);
glDeleteVertexArrays(1, &vao);
glDeleteTextures(1, &output_texture);
glDeleteFramebuffers(1, &framebuffer);
glDeleteProgram(program);
SDL_GL_DeleteContext(gl_context);
SDL_DestroyWindow(window);
SDL_Quit();