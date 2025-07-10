#include "SDL2/SDL.h"
#include "SDL2/SDL_syswm.h"
#include <iostream>
#include <cmath>
#include <vector>

#include <GLES3/gl3.h>
#include <EGL/egl.h>

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    auto window = SDL_CreateWindow("Offscreen", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 256, 1, SDL_WINDOW_HIDDEN);

    EGLDisplay eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL_NO_DISPLAY) {
        std::cerr << "EGL: Failed to get EGL display" << std::endl;
        return 1;
    }

    EGLint majorVersion, minorVersion;
    if (!eglInitialize(eglDisplay, &majorVersion, &minorVersion)) {
        std::cerr << "EGL: Failed to initialize EGL" << std::endl;
        return 1;
    }

    // Choose EGL config
    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE
    };

    EGLConfig eglConfig = nullptr;
    EGLint numConfigs;
    if (!eglChooseConfig(eglDisplay, configAttribs, &eglConfig, 1, &numConfigs)) {
        std::cerr << "EGL: Failed to choose EGL config" << std::endl;
        return 1;
    }

    if (numConfigs == 0) {
        std::cerr << "EGL: No suitable EGL config found" << std::endl;
        return 1;
    }

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
        std::cerr << "EGL: Failed to get window WM info" << std::endl;
        return 1;
    }

    EGLSurface eglSurface = eglCreateWindowSurface(eglDisplay, eglConfig, (EGLNativeWindowType)wmInfo.info.x11.window, nullptr);
    if (eglSurface == EGL_NO_SURFACE) {
        std::cerr << "EGL: Failed to create EGL surface" << std::endl;
        return 1;
    }

    // Create EGL context with OpenGL ES 3.0
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    EGLContext eglContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttribs);
    if (eglContext == EGL_NO_CONTEXT) {
        std::cerr << "EGL: Failed to create EGL context" << std::endl;
        return 1;
    }

    if (!eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
        std::cerr << "EGL: Failed to make context current" << std::endl;
        return 1;
    }

    if (!eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
        std::cerr << "EGL: Failed to make context current" << std::endl;
        return 1;
    }

    // Create vertex and fragment shaders
    const char* vertexShaderSource = R"(
#version 300 es
precision mediump float;

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    TexCoord = texCoord;
}
)";

    const char* fragmentShaderSource = R"(
#version 300 es
precision highp float;

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;

in vec2 TexCoord;

// Invert the y coordinate
uniform sampler2D stream_audio_texture;
uniform int buffer_size;
uniform int sample_rate;
uniform int num_channels;

layout(std140) uniform global_time {
    int global_time_val;
};

layout(location = 0) out vec4 output_audio_texture;
layout(location = 1) out vec4 debug_audio_texture;

void main() {
    // Use buffer_size to create a simple pattern
    float sample_pos = TexCoord.x * float(buffer_size);
    float channel_pos = TexCoord.y * float(num_channels);
    
    // Create a simple sine wave using sample_rate
    float time_in_seconds = sample_pos / float(sample_rate);
    float sine_wave = sin(TWO_PI * 440.0 * time_in_seconds);

    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    
    // Output the sine wave
    output_audio_texture = vec4(sine_wave, sine_wave, sine_wave, 1.0) + stream_audio;
    
    // Debug output shows the parameters
    debug_audio_texture = vec4(
        float(buffer_size) / 1000.0,  // Normalized buffer size
        float(sample_rate) / 48000.0, // Normalized sample rate  
        float(num_channels) / 8.0,    // Normalized channel count
        1.0
    );
}
)";

    // Compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    // Check vertex shader compilation
    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Error compiling vertex shader: " << infoLog << std::endl;
        return 1;
    }

    // Compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    // Check fragment shader compilation
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Error compiling fragment shader: " << infoLog << std::endl;
        return 1;
    }

    // Create and link shader program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Check program linking
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Error linking shader program: " << infoLog << std::endl;
        return 1;
    }

    // Create vertex data for a full-screen quad
    float quad[] = {
        // Position    Texcoords
        -1.0f, -1.0f, 0.0f, 0.0f,  // Bottom-left
        -1.0f,  1.0f, 0.0f, 1.0f,  // Top-left
         1.0f, -1.0f, 1.0f, 0.0f,  // Bottom-right
         1.0f,  1.0f, 1.0f, 1.0f,  // Top-right
        -1.0f,  1.0f, 0.0f, 1.0f,  // Top-left
         1.0f, -1.0f, 1.0f, 0.0f   // Bottom-right
    };

    // Create VAO and VBO
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    // Set vertex attributes
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)(2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    // Create framebuffer for offscreen rendering
    GLuint framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    // Create output texture
    GLuint output_texture;
    glGenTextures(1, &output_texture);
    glBindTexture(GL_TEXTURE_2D, output_texture);
    
    // Configure the output texture parameters (matching texture2d parameter style)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    
    // Allocate memory for the output texture and initialize it with 0s
    std::vector<unsigned char> output_zero_data(256 * 1 * 4, 0); // RGBA for 256x1 texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, output_zero_data.data());
    
    // Check for OpenGL errors
    GLenum status = glGetError();
    if (status != GL_NO_ERROR) {
        std::cerr << "Error: OpenGL error in output texture creation" << std::endl;
        return 1;
    }
    
    // Unbind the output texture after configuration
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create debug texture
    GLuint debug_texture;
    glGenTextures(1, &debug_texture);
    glBindTexture(GL_TEXTURE_2D, debug_texture);
    
    // Configure the debug texture parameters (matching texture2d parameter style)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    
    // Allocate memory for the debug texture and initialize it with 0s
    std::vector<unsigned char> debug_zero_data(256 * 1 * 4, 0); // RGBA for 256x1 texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, debug_zero_data.data());
    
    // Check for OpenGL errors
    status = glGetError();
    if (status != GL_NO_ERROR) {
        std::cerr << "Error: OpenGL error in debug texture creation" << std::endl;
        return 1;
    }
    
    // Unbind the debug texture after configuration
    glBindTexture(GL_TEXTURE_2D, 0);

    // Attach both textures to framebuffer (no need to bind textures first)
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_texture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, debug_texture, 0);


    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer is not complete!" << std::endl;
        return 1;
    }

    // Set GL settings
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    // Set up multiple render targets
    GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, drawBuffers);

    // Set up uniforms that the shader expects
    glUseProgram(shaderProgram);
    
    // Set uniform values
    GLint buffer_size_loc = glGetUniformLocation(shaderProgram, "buffer_size");
    GLint sample_rate_loc = glGetUniformLocation(shaderProgram, "sample_rate");
    GLint num_channels_loc = glGetUniformLocation(shaderProgram, "num_channels");
    
    if (buffer_size_loc != -1) glUniform1i(buffer_size_loc, 256);
    if (sample_rate_loc != -1) glUniform1i(sample_rate_loc, 44100);
    if (num_channels_loc != -1) glUniform1i(num_channels_loc, 2);
    
    // Create and bind a dummy texture for stream_audio_texture
    //GLuint stream_texture;
    //glGenTextures(1, &stream_texture);
    //glBindTexture(GL_TEXTURE_2D, stream_texture);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    //// Bind the stream texture to texture unit 0
    //glActiveTexture(GL_TEXTURE0);
    //glBindTexture(GL_TEXTURE_2D, stream_texture);
    //GLint stream_texture_loc = glGetUniformLocation(shaderProgram, "stream_audio_texture");
    //if (stream_texture_loc != -1) glUniform1i(stream_texture_loc, 0);
    
    // Set up global_time uniform block
    GLint global_time_block = glGetUniformBlockIndex(shaderProgram, "global_time");
    if (global_time_block != GL_INVALID_INDEX) {
        int time_val = 0; // You can set this to current time if needed
        glUniformBlockBinding(shaderProgram, global_time_block, 0);
        
        // Create uniform buffer for global_time
        GLuint time_ubo;
        glGenBuffers(1, &time_ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, time_ubo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(int), &time_val, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, time_ubo);
    }

    // Render
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Read back the rendered data from the output texture (attachment 0)
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    std::vector<unsigned char> pixels(256 * 4); // RGBA for 256 pixels
    glReadPixels(0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // Print some sample values
    std::cout << "Rendered pixel values (first 10 pixels):" << std::endl;
    for (int i = 0; i < 10; ++i) {
        std::cout << "Pixel " << i << ": R=" << (int)pixels[i*4] 
                  << " G=" << (int)pixels[i*4+1] 
                  << " B=" << (int)pixels[i*4+2] 
                  << " A=" << (int)pixels[i*4+3] << std::endl;
    }

    // Cleanup
    glDeleteTextures(1, &output_texture);
    glDeleteTextures(1, &debug_texture);
    //glDeleteTextures(1, &stream_texture);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(shaderProgram);
    glDeleteShader(fragmentShader);
    glDeleteShader(vertexShader);

    // EGL cleanup
    eglDestroyContext(eglDisplay, eglContext);
    eglDestroySurface(eglDisplay, eglSurface);
    eglTerminate(eglDisplay);

    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "Render stage test completed successfully!" << std::endl;
    return 0;
}