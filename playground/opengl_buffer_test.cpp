#include <SDL2/SDL.h>
#define GLEW_STATIC
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <string>
#include <iostream>
#include <vector>
#include <cassert>

// Window settings
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const char* WINDOW_TITLE = "OpenGL Buffer Test";

// Shader sources
const char* vertexShaderSource = R"(
#version 150
#if __VERSION__ >= 330
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
#else
attribute vec3 aPos;
attribute vec2 aTexCoord;
#endif

varying vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
)";

const char* fragmentShaderSource = R"(
#version 150
#if __VERSION__ >= 330
out vec4 FragColor;
#endif

varying vec2 TexCoord;

uniform sampler2D inputTexture;
#if __VERSION__ >= 330
layout(std140) uniform TestBlock {
    float intensity;
    vec4 tintColor;
};
#else
uniform float intensity;
uniform vec4 tintColor;
#endif

void main() {
    vec4 texColor = texture2D(inputTexture, TexCoord);
    vec4 result = mix(texColor, tintColor, intensity);
    
#if __VERSION__ >= 330
    FragColor = result;
#else
    gl_FragColor = result;
#endif
}
)";

// Simple error checking function
void checkGLError(const char* operation) {
    GLenum error;
    while ((error = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL error after " << operation << ": 0x" << std::hex << error << std::dec << std::endl;
    }
}

// Create and compile a shader
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation error: " << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

// Create shader program from vertex and fragment shaders
GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    
    if (!vertexShader || !fragmentShader) {
        return 0;
    }
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Shader program linking error: " << infoLog << std::endl;
        glDeleteProgram(program);
        return 0;
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
}

// Create a 2D texture with checkerboard pattern
GLuint createCheckerboardTexture(int width, int height) {
    std::vector<unsigned char> pixels(width * height * 4);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = (y * width + x) * 4;
            bool isEven = ((x / 32) % 2 == 0) ^ ((y / 32) % 2 == 0);
            
            pixels[index + 0] = isEven ? 255 : 0;   // R
            pixels[index + 1] = isEven ? 0 : 255;   // G
            pixels[index + 2] = 0;                  // B
            pixels[index + 3] = 255;                // A
        }
    }
    
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    
    checkGLError("creating texture");
    return texture;
}

// Create a framebuffer with color and depth attachments
GLuint createFramebuffer(GLuint& outputTexture, int width, int height) {
    GLuint framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    
    // Create color attachment texture
    glGenTextures(1, &outputTexture);
    glBindTexture(GL_TEXTURE_2D, outputTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTexture, 0);
    
    // Create renderbuffer for depth
    GLuint rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
    
    // Check framebuffer status
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer not complete! Status: 0x" << std::hex << status << std::dec << std::endl;
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteTextures(1, &outputTexture);
        glDeleteRenderbuffers(1, &rbo);
        return 0;
    }
    
    // Bind back to default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    checkGLError("creating framebuffer");
    return framebuffer;
}

// Create a pixel buffer object for reading back texture data
GLuint createPixelBufferObject(int size) {
    GLuint pbo;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, size, nullptr, GL_STREAM_READ);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    
    checkGLError("creating pixel buffer");
    return pbo;
}

// Create a uniform buffer object
GLuint createUniformBuffer(float intensity, float r, float g, float b, float a) {
    // Align to std140 layout
    struct {
        float intensity;
        float padding[3]; // Padding to match std140 layout rules
        float color[4];   // vec4 is 16-byte aligned
    } uboData;
    
    uboData.intensity = intensity;
    uboData.color[0] = r;
    uboData.color[1] = g;
    uboData.color[2] = b;
    uboData.color[3] = a;
    
    GLuint ubo;
    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(uboData), &uboData, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    
    checkGLError("creating uniform buffer");
    return ubo;
}

int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    // Set OpenGL attributes for compatibility with Mac and Raspberry Pi
#if defined(__APPLE__)
    // Mac-specific attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#elif defined(__RASPBERRY_PI__)
    // Raspberry Pi-specific attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    // Default attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
    
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    
    // Create window
    SDL_Window* window = SDL_CreateWindow(
        WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    
    if (!window) {
        std::cerr << "Window creation error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    
    // Create OpenGL context
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        std::cerr << "OpenGL context creation error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync
    
    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) {
        std::cerr << "GLEW initialization error: " << glewGetErrorString(glewError) << std::endl;
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Print OpenGL info
    std::cout << "OpenGL Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    
    // Check for required extensions
    bool has_pbo = glewIsSupported("GL_ARB_pixel_buffer_object") || 
                   glewIsSupported("GL_EXT_pixel_buffer_object");
    bool has_ubo = glewIsSupported("GL_ARB_uniform_buffer_object");
    bool has_fbo = glewIsSupported("GL_ARB_framebuffer_object") || 
                   glewIsSupported("GL_EXT_framebuffer_object");
    
    std::cout << "PBO Support: " << (has_pbo ? "Yes" : "No") << std::endl;
    std::cout << "UBO Support: " << (has_ubo ? "Yes" : "No") << std::endl;
    std::cout << "FBO Support: " << (has_fbo ? "Yes" : "No") << std::endl;
    
    if (!has_ubo || !has_fbo) {
        std::cerr << "Required OpenGL features not supported!" << std::endl;
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Create shader program
    GLuint shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    if (!shaderProgram) {
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Set up vertex data for a quad that covers the screen
    float vertices[] = {
        // positions         // texture coords
        -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
         1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f
    };
    
    unsigned int indices[] = {
        0, 1, 2,
        0, 2, 3
    };
    
    // Create and bind VAO, VBO, EBO
    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    
    glBindVertexArray(VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Texture coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
    
    // Create checkerboard texture
    GLuint inputTexture = createCheckerboardTexture(256, 256);
    
    // Create framebuffer and its texture
    GLuint outputTexture;
    GLuint framebuffer = createFramebuffer(outputTexture, WINDOW_WIDTH, WINDOW_HEIGHT);
    if (!framebuffer) {
        glDeleteTextures(1, &inputTexture);
        glDeleteProgram(shaderProgram);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        glDeleteVertexArrays(1, &VAO);
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Create uniform buffer object
    GLuint ubo = createUniformBuffer(0.5f, 0.0f, 0.0f, 1.0f, 0.7f);
    
    // Get uniform block index and bind it
    GLuint blockIndex = glGetUniformBlockIndex(shaderProgram, "TestBlock");
    glUniformBlockBinding(shaderProgram, blockIndex, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
    
    // Create pixel buffer object for readback
    GLuint pbo = 0;
    if (has_pbo) {
        pbo = createPixelBufferObject(WINDOW_WIDTH * WINDOW_HEIGHT * 4);
    }
    
    // Main loop
    bool quit = false;
    float intensity = 0.5f;
    SDL_Event event;
    
    while (!quit) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    quit = true;
                } else if (event.key.keysym.sym == SDLK_UP) {
                    intensity = std::min(1.0f, intensity + 0.1f);
                    // Update uniform buffer
                    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
                    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(float), &intensity);
                    glBindBuffer(GL_UNIFORM_BUFFER, 0);
                } else if (event.key.keysym.sym == SDLK_DOWN) {
                    intensity = std::max(0.0f, intensity - 0.1f);
                    // Update uniform buffer
                    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
                    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(float), &intensity);
                    glBindBuffer(GL_UNIFORM_BUFFER, 0);
                }
            }
        }
        
        // First pass: Render to framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
        
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(shaderProgram);
        
        // Bind the input texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, inputTexture);
        glUniform1i(glGetUniformLocation(shaderProgram, "inputTexture"), 0);
        
        // Draw the quad
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        
        // If PBO is supported, initiate asynchronous readback
        if (has_pbo && pbo) {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
            glReadPixels(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, 0);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }
        
        // Second pass: Render to screen
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
        
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Bind the framebuffer's texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, outputTexture);
        
        // Draw the quad again
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        
        // Swap the window buffers
        SDL_GL_SwapWindow(window);
        
        // Optional: print readback from PBO
        if (has_pbo && pbo && false) { // Set to true to enable readback logging
            glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
            void* data = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
            
            if (data) {
                unsigned char* pixels = static_cast<unsigned char*>(data);
                // Print the first few pixels (RGBA values)
                std::cout << "First pixel: R=" << (int)pixels[0] 
                          << " G=" << (int)pixels[1]
                          << " B=" << (int)pixels[2]
                          << " A=" << (int)pixels[3] << std::endl;
                
                glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            }
            
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }
        
        // Small delay to reduce CPU usage
        SDL_Delay(10);
    }
    
    // Cleanup
    if (pbo) {
        glDeleteBuffers(1, &pbo);
    }
    
    glDeleteBuffers(1, &ubo);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &outputTexture);
    glDeleteTextures(1, &inputTexture);
    glDeleteProgram(shaderProgram);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &VAO);
    
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}