#include <SDL2/SDL.h>
#include <iostream>
#include <GLES3/gl3.h>

// ============================================================================
// AGENT CONFLICT TEST CASE - Multiple agents should modify different sections
// ============================================================================
// This file is designed to test merge conflict resolution when multiple
// agents make overlapping changes. Three agents should modify:
//   Agent 1: Window dimensions (WINDOW_WIDTH, WINDOW_HEIGHT)
//   Agent 2: Window title (WINDOW_TITLE)
//   Agent 3: Background clear color (CLEAR_RED, CLEAR_GREEN, CLEAR_BLUE)
// ============================================================================

// Agent 1 Section: Window Dimensions
// AGENT_1_TARGET_START
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
// AGENT_1_TARGET_END

// Agent 2 Section: Window Title
// AGENT_2_TARGET_START
const char* WINDOW_TITLE = "OpenGL Empty Window";
// AGENT_2_TARGET_END

// Agent 3 Section: Background Clear Color
// AGENT_3_TARGET_START
const float CLEAR_RED = 0.2f;
const float CLEAR_GREEN = 0.3f;
const float CLEAR_BLUE = 0.3f;
const float CLEAR_ALPHA = 1.0f;
// AGENT_3_TARGET_END

void framebuffer_size_callback(SDL_Window* window, int width, int height) {
    glViewport(0, 0, width, height);
}

int main() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Set OpenGL attributes for OpenGL ES 3.0
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    // Create window using dimensions from Agent 1 section
    SDL_Window* window = SDL_CreateWindow(
        WINDOW_TITLE,  // Window title from Agent 2 section
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,  // Dimensions from Agent 1 section
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    // Create OpenGL context
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "OpenGL context creation failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Print OpenGL information
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "Window Title: " << WINDOW_TITLE << std::endl;
    std::cout << "Window Size: " << WINDOW_WIDTH << "x" << WINDOW_HEIGHT << std::endl;

    // Main render loop
    bool running = true;
    SDL_Event event;
    
    while (running) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    framebuffer_size_callback(window, event.window.data1, event.window.data2);
                }
            }
        }

        // Clear with color from Agent 3 section
        glClearColor(CLEAR_RED, CLEAR_GREEN, CLEAR_BLUE, CLEAR_ALPHA);
        glClear(GL_COLOR_BUFFER_BIT);

        // Swap buffers
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
