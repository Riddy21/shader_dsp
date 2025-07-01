#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>

// Shader sources
const char* vertexShaderSource = R"(
#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

out vec3 ourColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;

void main()
{
    // Add some animation
    vec3 pos = aPos;
    pos.y += sin(time + aPos.x * 2.0) * 0.1;
    
    gl_Position = projection * view * model * vec4(pos, 1.0);
    ourColor = aColor;
}
)";

const char* fragmentShaderSource = R"(
#version 460 core
out vec4 FragColor;
in vec3 ourColor;

uniform float time;

void main()
{
    // Create a pulsing effect
    float pulse = (sin(time * 2.0) + 1.0) * 0.5;
    vec3 color = ourColor * (0.8 + pulse * 0.2);
    FragColor = vec4(color, 1.0);
}
)";

// Global variables
SDL_Window* window = nullptr;
SDL_GLContext glContext;
GLuint shaderProgram;
GLuint VAO, VBO;
GLuint modelMatrixLocation, viewMatrixLocation, projectionMatrixLocation, timeLocation;
float rotationAngle = 0.0f;
auto startTime = std::chrono::high_resolution_clock::now();
bool running = true;

// Function to check for OpenGL errors
void checkGLError(const char* operation) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error after " << operation << ": " << error << std::endl;
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

// Function to create a cube with colors
void createCube() {
    // Cube vertices with positions and colors
    float vertices[] = {
        // positions          // colors
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
        
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 0.5f,
        -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f
    };
    
    // Cube indices
    unsigned int indices[] = {
        0, 1, 2, 2, 3, 0,  // front
        1, 5, 6, 6, 2, 1,  // right
        5, 4, 7, 7, 6, 5,  // back
        4, 0, 3, 3, 7, 4,  // left
        3, 2, 6, 6, 7, 3,  // top
        4, 5, 1, 1, 0, 4   // bottom
    };
    
    GLuint EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    
    glBindVertexArray(VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
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
    checkGLError("createCube");
}

// Function to create matrices
void createMatrices(float* model, float* view, float* projection) {
    // Model matrix (rotation around Y axis)
    float cos_a = cos(rotationAngle);
    float sin_a = sin(rotationAngle);
    
    model[0] = cos_a;  model[1] = 0.0f;   model[2] = sin_a;   model[3] = 0.0f;
    model[4] = 0.0f;   model[5] = 1.0f;   model[6] = 0.0f;    model[7] = 0.0f;
    model[8] = -sin_a; model[9] = 0.0f;   model[10] = cos_a;  model[11] = 0.0f;
    model[12] = 0.0f;  model[13] = 0.0f;  model[14] = 0.0f;   model[15] = 1.0f;
    
    // View matrix (camera at (0, 0, 3) looking at origin)
    view[0] = 1.0f; view[1] = 0.0f; view[2] = 0.0f;  view[3] = 0.0f;
    view[4] = 0.0f; view[5] = 1.0f; view[6] = 0.0f;  view[7] = 0.0f;
    view[8] = 0.0f; view[9] = 0.0f; view[10] = 1.0f; view[11] = 0.0f;
    view[12] = 0.0f; view[13] = 0.0f; view[14] = -3.0f; view[15] = 1.0f;
    
    // Projection matrix (perspective)
    float fov = 45.0f * 3.14159f / 180.0f;
    float aspect = 800.0f / 600.0f;
    float near = 0.1f;
    float far = 100.0f;
    float f = 1.0f / tan(fov / 2.0f);
    
    projection[0] = f / aspect; projection[1] = 0.0f; projection[2] = 0.0f; projection[3] = 0.0f;
    projection[4] = 0.0f; projection[5] = f; projection[6] = 0.0f; projection[7] = 0.0f;
    projection[8] = 0.0f; projection[9] = 0.0f; projection[10] = (far + near) / (near - far); projection[11] = -1.0f;
    projection[12] = 0.0f; projection[13] = 0.0f; projection[14] = (2.0f * far * near) / (near - far); projection[15] = 0.0f;
}

// Function to render the scene
void render() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
    float time = duration.count() / 1000.0f;
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    
    glUseProgram(shaderProgram);
    
    // Update rotation
    rotationAngle += 0.01f;
    
    // Create matrices
    float model[16], view[16], projection[16];
    createMatrices(model, view, projection);
    
    // Set uniforms
    glUniformMatrix4fv(modelMatrixLocation, 1, GL_FALSE, model);
    glUniformMatrix4fv(viewMatrixLocation, 1, GL_FALSE, view);
    glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, projection);
    glUniform1f(timeLocation, time);
    
    // Draw the cube
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    
    SDL_GL_SwapWindow(window);
    checkGLError("render");
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
                        rotationAngle = 0.0f;
                        break;
                }
                break;
        }
    }
}

// Function to initialize SDL and OpenGL
bool init() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    
    // Create window
    window = SDL_CreateWindow(
        "OpenGL 4.6 Playground with SDL2",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create OpenGL context
    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "OpenGL context could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return false;
    }
    
    // Print OpenGL version
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "OpenGL Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    
    // Create shader program
    shaderProgram = createShaderProgram();
    
    // Get uniform locations
    modelMatrixLocation = glGetUniformLocation(shaderProgram, "model");
    viewMatrixLocation = glGetUniformLocation(shaderProgram, "view");
    projectionMatrixLocation = glGetUniformLocation(shaderProgram, "projection");
    timeLocation = glGetUniformLocation(shaderProgram, "time");
    
    // Create geometry
    createCube();
    
    checkGLError("init");
    return true;
}

// Function to cleanup resources
void cleanup() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int main(int argc, char** argv) {
    if (!init()) {
        std::cerr << "Failed to initialize!" << std::endl;
        return 1;
    }
    
    std::cout << "OpenGL Playground Started!" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  ESC - Exit" << std::endl;
    std::cout << "  R   - Reset rotation" << std::endl;
    
    // Main loop
    while (running) {
        handleEvents();
        render();
        SDL_Delay(16); // ~60 FPS
    }
    
    cleanup();
    return 0;
} 