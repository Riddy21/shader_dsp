#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <cmath>

// Window and context structure
struct WindowContext {
    SDL_Window* window;
    EGLDisplay eglDisplay;
    EGLContext eglContext;
    EGLSurface eglSurface;
    GLuint shaderProgram;
    GLuint VAO, VBO;
    GLuint modelMatrixLocation, viewMatrixLocation, projectionMatrixLocation;
    float rotationAngle;
    std::string title;
    int windowType; // 0=triangle, 1=square, 2=circle
};

// Global variables
std::vector<WindowContext> windows;
bool running = true;

// FPS tracking
std::vector<double> frameTimes;
auto lastFrameTime = std::chrono::high_resolution_clock::now();
int frameCount = 0;

// Vertex shader for ES 3.0
const char* vertexShaderSource = R"(
#version 300 es
precision mediump float;

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

out vec3 ourColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    ourColor = aColor;
}
)";

// Fragment shader for ES 3.0
const char* fragmentShaderSource = R"(
#version 300 es
precision mediump float;

out vec4 FragColor;
in vec3 ourColor;

void main()
{
    FragColor = vec4(ourColor, 1.0);
}
)";

// Function to check for OpenGL errors
void checkGLError(const char* operation) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error after " << operation << ": " << error << std::endl;
    }
}

// Function to check for EGL errors
void checkEGLError(const char* operation) {
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        std::cerr << "EGL error after " << operation << ": " << error << std::endl;
    }
}

// Function to compile shader
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    // Check for compilation errors
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation error: " << infoLog << std::endl;
    }
    
    return shader;
}

// Function to create and link shader program
GLuint createShaderProgram() {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    // Check for linking errors
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Shader program linking error: " << infoLog << std::endl;
    }
    
    // Clean up shaders
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
}

// Function to create triangle geometry
void createTriangle(WindowContext& ctx) {
    // Triangle vertices with positions and colors
    float vertices[] = {
        // positions          // colors
        -0.5f, -0.5f, 0.0f,   1.0f, 0.0f, 0.0f,  // Red
         0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,  // Green
         0.0f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f   // Blue
    };
    
    glGenVertexArrays(1, &ctx.VAO);
    glGenBuffers(1, &ctx.VBO);
    
    glBindVertexArray(ctx.VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, ctx.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
    checkGLError("createTriangle");
}

// Function to create square geometry
void createSquare(WindowContext& ctx) {
    // Square vertices with positions and colors
    float vertices[] = {
        // positions          // colors
        -0.5f, -0.5f, 0.0f,   1.0f, 1.0f, 0.0f,  // Yellow
         0.5f, -0.5f, 0.0f,   1.0f, 0.0f, 1.0f,  // Magenta
         0.5f,  0.5f, 0.0f,   0.0f, 1.0f, 1.0f,  // Cyan
        -0.5f,  0.5f, 0.0f,   1.0f, 0.5f, 0.0f   // Orange
    };
    
    unsigned int indices[] = {
        0, 1, 2,  // First triangle
        2, 3, 0   // Second triangle
    };
    
    glGenVertexArrays(1, &ctx.VAO);
    glGenBuffers(1, &ctx.VBO);
    GLuint EBO;
    glGenBuffers(1, &EBO);
    
    glBindVertexArray(ctx.VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, ctx.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
    checkGLError("createSquare");
}

// Function to create circle geometry
void createCircle(WindowContext& ctx) {
    const int segments = 32;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    
    // Center vertex
    vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(0.0f);
    vertices.push_back(1.0f); vertices.push_back(1.0f); vertices.push_back(1.0f); // White center
    
    // Circle vertices
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * M_PI * float(i) / float(segments);
        float x = 0.5f * cos(angle);
        float y = 0.5f * sin(angle);
        
        vertices.push_back(x); vertices.push_back(y); vertices.push_back(0.0f);
        vertices.push_back(0.5f); vertices.push_back(0.0f); vertices.push_back(1.0f); // Purple
    }
    
    // Create indices for triangle fan
    for (int i = 1; i <= segments; i++) {
        indices.push_back(0);
        indices.push_back(i);
        indices.push_back(i + 1);
    }
    
    glGenVertexArrays(1, &ctx.VAO);
    glGenBuffers(1, &ctx.VBO);
    GLuint EBO;
    glGenBuffers(1, &EBO);
    
    glBindVertexArray(ctx.VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, ctx.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
    checkGLError("createCircle");
}

// Function to create matrices
void createMatrices(float* model, float* view, float* projection, float rotationAngle) {
    // Model matrix (rotation around Z axis)
    float cos_a = cos(rotationAngle);
    float sin_a = sin(rotationAngle);
    
    model[0] = cos_a;  model[1] = -sin_a; model[2] = 0.0f;   model[3] = 0.0f;
    model[4] = sin_a;  model[5] = cos_a;  model[6] = 0.0f;   model[7] = 0.0f;
    model[8] = 0.0f;   model[9] = 0.0f;   model[10] = 1.0f;  model[11] = 0.0f;
    model[12] = 0.0f;  model[13] = 0.0f;  model[14] = 0.0f;  model[15] = 1.0f;
    
    // View matrix (simple orthographic view)
    view[0] = 1.0f; view[1] = 0.0f; view[2] = 0.0f;  view[3] = 0.0f;
    view[4] = 0.0f; view[5] = 1.0f; view[6] = 0.0f;  view[7] = 0.0f;
    view[8] = 0.0f; view[9] = 0.0f; view[10] = 1.0f; view[11] = 0.0f;
    view[12] = 0.0f; view[13] = 0.0f; view[14] = 0.0f; view[15] = 1.0f;
    
    // Projection matrix (orthographic)
    projection[0] = 1.0f; projection[1] = 0.0f; projection[2] = 0.0f; projection[3] = 0.0f;
    projection[4] = 0.0f; projection[5] = 1.0f; projection[6] = 0.0f; projection[7] = 0.0f;
    projection[8] = 0.0f; projection[9] = 0.0f; projection[10] = -1.0f; projection[11] = 0.0f;
    projection[12] = 0.0f; projection[13] = 0.0f; projection[14] = 0.0f; projection[15] = 1.0f;
}

// Function to calculate and display FPS
void updateFPS() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto frameTime = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - lastFrameTime);
    double frameTimeMs = frameTime.count() / 1000.0;
    
    frameTimes.push_back(frameTimeMs);
    if (frameTimes.size() > 60) { // Keep last 60 frames
        frameTimes.erase(frameTimes.begin());
    }
    
    frameCount++;
    if (frameCount % 60 == 0) { // Print FPS every 60 frames
        double avgFrameTime = 0.0;
        for (double time : frameTimes) {
            avgFrameTime += time;
        }
        avgFrameTime /= frameTimes.size();
        double fps = 1000.0 / avgFrameTime;
        
        std::cout << "FPS: " << fps << " (Avg frame time: " << avgFrameTime << "ms)" << std::endl;
    }
    
    lastFrameTime = currentTime;
}

// Function to render a window
void renderWindow(WindowContext& ctx) {
    // Make this context current
    if (!eglMakeCurrent(ctx.eglDisplay, ctx.eglSurface, ctx.eglSurface, ctx.eglContext)) {
        std::cerr << "Failed to make context current for " << ctx.title << std::endl;
        return;
    }
    
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Different background colors for each window
    switch (ctx.windowType) {
        case 0: // Triangle
            glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
            break;
        case 1: // Square
            glClearColor(0.2f, 0.1f, 0.1f, 1.0f);
            break;
        case 2: // Circle
            glClearColor(0.1f, 0.2f, 0.1f, 1.0f);
            break;
    }
    
    glUseProgram(ctx.shaderProgram);
    
    // Update rotation (different speeds for each window)
    switch (ctx.windowType) {
        case 0: // Triangle
            ctx.rotationAngle += 0.02f;
            break;
        case 1: // Square
            ctx.rotationAngle += 0.015f;
            break;
        case 2: // Circle
            ctx.rotationAngle += 0.01f;
            break;
    }
    
    // Create matrices
    float model[16], view[16], projection[16];
    createMatrices(model, view, projection, ctx.rotationAngle);
    
    // Set uniforms
    glUniformMatrix4fv(ctx.modelMatrixLocation, 1, GL_FALSE, model);
    glUniformMatrix4fv(ctx.viewMatrixLocation, 1, GL_FALSE, view);
    glUniformMatrix4fv(ctx.projectionMatrixLocation, 1, GL_FALSE, projection);
    
    // Draw the geometry
    glBindVertexArray(ctx.VAO);
    
    switch (ctx.windowType) {
        case 0: // Triangle
            glDrawArrays(GL_TRIANGLES, 0, 3);
            break;
        case 1: // Square
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            break;
        case 2: // Circle
            glDrawElements(GL_TRIANGLES, 96, GL_UNSIGNED_INT, 0); // 32 segments * 3 vertices
            break;
    }
    
    eglSwapBuffers(ctx.eglDisplay, ctx.eglSurface);
    checkGLError("renderWindow");
}

// Function to handle SDL events
void handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    case SDLK_r:
                        for (auto& ctx : windows) {
                            ctx.rotationAngle = 0.0f;
                        }
                        frameTimes.clear();
                        frameCount = 0;
                        std::cout << "Reset rotation and FPS tracking" << std::endl;
                        break;
                }
                break;
        }
    }
}

// Function to initialize a single window and EGL context
bool initWindow(WindowContext& ctx, int x, int y, int width, int height, const std::string& title, int windowType) {
    ctx.windowType = windowType;
    ctx.title = title;
    ctx.rotationAngle = 0.0f;
    
    // Create window
    ctx.window = SDL_CreateWindow(
        title.c_str(),
        x, y,
        width, height,
        SDL_WINDOW_SHOWN
    );
    
    if (!ctx.window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Get native window handle
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(ctx.window, &wmInfo)) {
        std::cerr << "Failed to get window WM info: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Initialize EGL
    ctx.eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (ctx.eglDisplay == EGL_NO_DISPLAY) {
        std::cerr << "Failed to get EGL display for " << title << std::endl;
        return false;
    }
    
    EGLint major, minor;
    if (!eglInitialize(ctx.eglDisplay, &major, &minor)) {
        std::cerr << "Failed to initialize EGL for " << title << std::endl;
        return false;
    }
    
    // Configure EGL
    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
    
    EGLConfig config;
    EGLint numConfigs;
    if (!eglChooseConfig(ctx.eglDisplay, configAttribs, &config, 1, &numConfigs)) {
        std::cerr << "Failed to choose EGL config for " << title << std::endl;
        return false;
    }
    
    // Create EGL surface - handle different window systems
    EGLNativeWindowType nativeWindow;
    
    #if defined(SDL_VIDEO_DRIVER_X11)
        nativeWindow = (EGLNativeWindowType)wmInfo.info.x11.window;
    #elif defined(SDL_VIDEO_DRIVER_WAYLAND)
        nativeWindow = (EGLNativeWindowType)wmInfo.info.wl.surface;
    #elif defined(SDL_VIDEO_DRIVER_WINDOWS)
        nativeWindow = (EGLNativeWindowType)wmInfo.info.win.window;
    #else
        std::cerr << "Unsupported video driver for EGL" << std::endl;
        return false;
    #endif
    
    ctx.eglSurface = eglCreateWindowSurface(ctx.eglDisplay, config, nativeWindow, nullptr);
    if (ctx.eglSurface == EGL_NO_SURFACE) {
        std::cerr << "Failed to create EGL surface for " << title << std::endl;
        return false;
    }
    
    // Create EGL context
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    
    ctx.eglContext = eglCreateContext(ctx.eglDisplay, config, EGL_NO_CONTEXT, contextAttribs);
    if (ctx.eglContext == EGL_NO_CONTEXT) {
        std::cerr << "Failed to create EGL context for " << title << std::endl;
        return false;
    }
    
    // Make current
    if (!eglMakeCurrent(ctx.eglDisplay, ctx.eglSurface, ctx.eglSurface, ctx.eglContext)) {
        std::cerr << "Failed to make EGL context current for " << title << std::endl;
        return false;
    }
    
    // Disable vsync for maximum FPS
    eglSwapInterval(ctx.eglDisplay, 0);
    
    // Create shader program
    ctx.shaderProgram = createShaderProgram();
    
    // Get uniform locations
    ctx.modelMatrixLocation = glGetUniformLocation(ctx.shaderProgram, "model");
    ctx.viewMatrixLocation = glGetUniformLocation(ctx.shaderProgram, "view");
    ctx.projectionMatrixLocation = glGetUniformLocation(ctx.shaderProgram, "projection");
    
    // Create geometry based on window type
    switch (windowType) {
        case 0:
            createTriangle(ctx);
            break;
        case 1:
            createSquare(ctx);
            break;
        case 2:
            createCircle(ctx);
            break;
    }
    
    checkGLError("initWindow");
    return true;
}

// Function to initialize all windows
bool initEGL() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create 3 windows
    windows.resize(3);
    
    if (!initWindow(windows[0], 100, 100, 400, 300, "Triangle Window", 0)) return false;
    if (!initWindow(windows[1], 550, 100, 400, 300, "Square Window", 1)) return false;
    if (!initWindow(windows[2], 100, 450, 400, 300, "Circle Window", 2)) return false;
    
    std::cout << "Created 3 windows with separate EGL contexts:" << std::endl;
    std::cout << "  - Triangle Window (red/green/blue triangle)" << std::endl;
    std::cout << "  - Square Window (yellow/magenta/cyan/orange square)" << std::endl;
    std::cout << "  - Circle Window (white center, purple border)" << std::endl;
    
    return true;
}

// Function to cleanup resources
void cleanup() {
    for (auto& ctx : windows) {
        // Make context current for cleanup
        eglMakeCurrent(ctx.eglDisplay, ctx.eglSurface, ctx.eglSurface, ctx.eglContext);
        
        glDeleteVertexArrays(1, &ctx.VAO);
        glDeleteBuffers(1, &ctx.VBO);
        glDeleteProgram(ctx.shaderProgram);
        
        eglMakeCurrent(ctx.eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(ctx.eglDisplay, ctx.eglSurface);
        eglDestroyContext(ctx.eglDisplay, ctx.eglContext);
        eglTerminate(ctx.eglDisplay);
        
        SDL_DestroyWindow(ctx.window);
    }
    
    SDL_Quit();
}

int main(int argc, char** argv) {
    if (!initEGL()) {
        std::cerr << "Failed to initialize!" << std::endl;
        return 1;
    }
    
    std::cout << "Multi-Window EGL OpenGL ES 3.0 FPS Test Started!" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  ESC - Exit" << std::endl;
    std::cout << "  R   - Reset rotation and FPS tracking" << std::endl;
    std::cout << "FPS will be displayed every 60 frames..." << std::endl;
    
    // Main loop
    while (running) {
        handleEvents();
        
        // Render each window
        for (auto& ctx : windows) {
            renderWindow(ctx);
        }
        
        updateFPS();
        SDL_Delay(1); // Minimal delay to prevent excessive CPU usage
    }
    
    cleanup();
    return 0;
} 