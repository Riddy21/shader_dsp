#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <iostream>
#include <thread>
#include <mutex>

// Mutex for synchronizing access to SDL and OpenGL resources
std::mutex glMutex;

// A simple function to initialize and render a window with OpenGL
void renderWindow(int windowId, const std::string& windowName) {
    SDL_Init(SDL_INIT_VIDEO);

    // Create a window
    SDL_Window* window = SDL_CreateWindow(windowName.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL);
    if (!window) {
        std::cerr << "SDL CreateWindow Error: " << SDL_GetError() << std::endl;
        return;
    }

    // Create an OpenGL context for the window
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        std::cerr << "SDL_GL_CreateContext Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        return;
    }

    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "GLEW Initialization Error" << std::endl;
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        return;
    }

    glEnable(GL_DEPTH_TEST);  // Enable depth testing for 3D rendering

    // Render loop
    bool isRunning = true;
    while (isRunning) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                isRunning = false;
            }
        }

        // Clear the screen with a color (different for each window)
        if (windowId == 1) {
            glClearColor(0.2f, 0.3f, 0.3f, 1.0f);  // Dark blue
        } else {
            glClearColor(0.3f, 0.2f, 0.3f, 1.0f);  // Purple
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Swap buffers to display the rendered image
        SDL_GL_SwapWindow(window);
        SDL_Delay(16);  // Frame delay to limit FPS
    }

    // Clean up
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

// Function to run two windows on separate threads
void createWindows() {
    std::thread window1(renderWindow, 1, "Window 1 - Blue Background");
    std::thread window2(renderWindow, 2, "Window 2 - Purple Background");

    window1.join();
    window2.join();
}

int main() {
    createWindows();
    return 0;
}
