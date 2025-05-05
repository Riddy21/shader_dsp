#include "graphics/graphics_display.h"
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <iostream>
#include <thread>
#include <string>

GLuint m_shaderProgram;

GraphicsDisplay::GraphicsDisplay(unsigned int width, unsigned int height, const std::string& title)
    : m_width(width), m_height(height), m_title(title) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        throw std::runtime_error("SDL initialization failed");
    }

    m_window = SDL_CreateWindow(
        m_title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        m_width,
        m_height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    if (!m_window) {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        throw std::runtime_error("SDL window creation failed");
    }

    m_context = SDL_GL_CreateContext(m_window);
    if (!m_context) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(m_window);
        SDL_Quit();
        throw std::runtime_error("OpenGL context creation failed");
    }

    SDL_GL_MakeCurrent(m_window, m_context); // Bind context to the main thread initially

    if (SDL_GL_SetSwapInterval(0) < 0) { // Disable VSync
        std::cerr << "Warning: Unable to disable VSync: " << SDL_GetError() << std::endl;
    }

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        SDL_GL_DeleteContext(m_context);
        SDL_DestroyWindow(m_window);
        SDL_Quit();
        throw std::runtime_error("GLEW initialization failed");
    }

    // Vertex shader source
    const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec3 aColor;

        out vec3 vertexColor;

        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            vertexColor = aColor;
        }
    )";

    // Fragment shader source
    const char* fragmentShaderSource = R"(
        #version 330 core
        in vec3 vertexColor;
        out vec4 FragColor;

        void main() {
            FragColor = vec4(vertexColor, 1.0);
        }
    )";

    // Compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    // Check for vertex shader compilation errors
    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Error: Vertex shader compilation failed\n" << infoLog << std::endl;
    }

    // Compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    // Check for fragment shader compilation errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Error: Fragment shader compilation failed\n" << infoLog << std::endl;
    }

    // Link shaders into a program
    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vertexShader);
    glAttachShader(m_shaderProgram, fragmentShader);
    glLinkProgram(m_shaderProgram);

    // Check for linking errors
    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Error: Shader program linking failed\n" << infoLog << std::endl;
    }

    // Delete shaders as they are no longer needed
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Initialize triangle data
    GLfloat vertices[] = {
        // Positions       // Colors
         0.0f,  0.5f, 1.0f, 0.0f, 0.0f, // Top
        -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, // Bottom-left
         0.5f, -0.5f, 0.0f, 0.0f, 1.0f  // Bottom-right
    };

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    auto& event_loop = EventLoop::get_instance();
    event_loop.add_loop_item(this); // Register this graphics display instance with the event loop
}

GraphicsDisplay::~GraphicsDisplay() {
    if (m_context) {
        SDL_GL_DeleteContext(m_context);
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
    }
    SDL_Quit();

    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
    glDeleteProgram(m_shaderProgram);
}


// Render graphics less often
bool GraphicsDisplay::is_ready() {
    return m_window != nullptr && m_context != nullptr;
}

void GraphicsDisplay::handle_event(const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        EventLoop::get_instance().terminate();
    }
}

void GraphicsDisplay::render() {
    SDL_GL_MakeCurrent(m_window, m_context); // Ensure this context is active

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Use the shader program
    glUseProgram(m_shaderProgram);

    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    SDL_GL_SwapWindow(m_window); // Swap buffers for this context
}
