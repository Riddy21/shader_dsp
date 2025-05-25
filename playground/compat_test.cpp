#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <stdio.h>
#include <vector>
#include <cassert>

#define WIDTH 512
#define HEIGHT 512

const char* vertex_shader_src = R"(
#version 150 core
in vec2 in_pos;
in vec2 in_uv;
out vec2 uv;
void main() {
    uv = in_uv;
    gl_Position = vec4(in_pos, 0, 1);
}
)";

const char* fragment_shader_src = R"(
#version 150 core
in vec2 uv;
out vec4 out_color;
layout(std140) uniform ColorBlock {
    vec4 baseColor;
};
void main() {
    // Simple gradient, mix with uniform color
    out_color = mix(vec4(uv, 0, 1), baseColor, 0.5);
}
)";

GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint status;
    glGetShaderiv(s, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[512];
        glGetShaderInfoLog(s, 512, nullptr, log);
        fprintf(stderr, "Shader error: %s\n", log);
        exit(1);
    }
    return s;
}

GLuint make_program(const char* vsrc, const char* fsrc) {
    GLuint v = compile_shader(GL_VERTEX_SHADER, vsrc);
    GLuint f = compile_shader(GL_FRAGMENT_SHADER, fsrc);
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glBindAttribLocation(p, 0, "in_pos");
    glBindAttribLocation(p, 1, "in_uv");
    glLinkProgram(p);
    GLint status;
    glGetProgramiv(p, GL_LINK_STATUS, &status);
    if (!status) {
        char log[512];
        glGetProgramInfoLog(p, 512, nullptr, log);
        fprintf(stderr, "Link error: %s\n", log);
        exit(1);
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

int main(int argc, char** argv) {
    // SDL2 + OpenGL context creation
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // Set OpenGL attributes more compatible with Raspberry Pi
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1); // Lower minor version
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    // Remove forward compatible flag for better compatibility
    // SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); 
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16); // Lower depth buffer size

    SDL_Window* window = SDL_CreateWindow(
        "OpenGL 3.1 Core Demo",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );
    printf("SDL_CreateWindow returned: %p\n", window);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    if (!ctx) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_MakeCurrent(window, ctx);
    
    // Print SDL GL attributes
    int major, minor;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
    printf("SDL reports OpenGL version: %d.%d\n", major, minor);

    // GLEW setup (must be after context creation)
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW error: %s\n", glewGetErrorString(err));
        return 1;
    }
    
    // Check for specific OpenGL versions
    printf("GLEW_VERSION_1_1: %s\n", GLEW_VERSION_1_1 ? "YES" : "NO");
    printf("GLEW_VERSION_2_0: %s\n", GLEW_VERSION_2_0 ? "YES" : "NO");
    printf("GLEW_VERSION_3_0: %s\n", GLEW_VERSION_3_0 ? "YES" : "NO");
    printf("GLEW_VERSION_3_2: %s\n", GLEW_VERSION_3_2 ? "YES" : "NO");
    printf("GLEW_VERSION_3_3: %s\n", GLEW_VERSION_3_3 ? "YES" : "NO");
    
    // Clear any error that might have been set by glewInit
    glGetError();
    
    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* glsl_version = glGetString(GL_SHADING_LANGUAGE_VERSION);
    
    printf("GL_VERSION  : %s\n", version ? (const char*)version : "(null)");
    printf("GLSL_VERSION: %s\n", glsl_version ? (const char*)glsl_version : "(null)");
    
    // Try an alternative way to check OpenGL version
    int gl_major_version = 0;
    int gl_minor_version = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &gl_major_version);
    glGetIntegerv(GL_MINOR_VERSION, &gl_minor_version);
    printf("GL version from glGetIntegerv: %d.%d\n", gl_major_version, gl_minor_version);

    // Compile/link shaders and create program
    GLuint program = make_program(vertex_shader_src, fragment_shader_src);

    // Uniform Buffer Object (UBO)
    GLuint ubo;
    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    float color_block[4] = {1.0f, 0.2f, 0.6f, 1.0f}; // RGBA
    glBufferData(GL_UNIFORM_BUFFER, sizeof(color_block), color_block, GL_DYNAMIC_DRAW);
    GLuint block_index = glGetUniformBlockIndex(program, "ColorBlock");
    glUniformBlockBinding(program, block_index, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);

    // Fullscreen quad: pos(x,y), uv(u,v)
    float verts[] = {
        //  pos      uv
        -1, -1,   0, 0,
         1, -1,   1, 0,
         1,  1,   1, 1,
        -1,  1,   0, 1
    };
    unsigned idx[] = {0,1,2, 2,3,0};
    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0); // in_pos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float))); // in_uv
    glEnableVertexAttribArray(1);

    // 2D Texture for FBO
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Framebuffer Object (FBO)
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    
    // Create a renderbuffer object for depth attachment (needed for FBO completeness)
    GLuint rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, WIDTH, HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
    
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Framebuffer incomplete! Status: 0x%x\n", status);
        return 1;
    }

    // Pixel Buffer Object (PBO) for reading pixels
    GLuint pbo;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, WIDTH * HEIGHT * 4, nullptr, GL_STREAM_READ);

    // Render to FBO
    glViewport(0, 0, WIDTH, HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(program);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // Read FBO pixels into PBO
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, 0);

    // Map PBO for CPU access and print first pixel
    GLubyte* ptr = (GLubyte*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    if (ptr) {
        printf("First pixel RGBA: %d %d %d %d\n", ptr[0], ptr[1], ptr[2], ptr[3]);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    } else {
        printf("Failed to map PBO\n");
    }

    // Render FBO texture to window (draw quad, sampling FBO texture)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, WIDTH, HEIGHT);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(program);
    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    SDL_GL_SwapWindow(window);

    // Keep window open for 2 seconds, then quit
    SDL_Delay(2000);

    // Cleanup
    glDeleteProgram(program);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &ubo);
    glDeleteTextures(1, &tex);
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rbo);
    glDeleteBuffers(1, &pbo);

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
