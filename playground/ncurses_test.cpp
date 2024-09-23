#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <iostream>

// Screen dimensions
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

void handle_keydown(SDL_Keycode key) {
    switch (key) {
        case SDLK_UP:
            std::cout << "Up arrow key pressed" << std::endl;
            break;
        case SDLK_DOWN:
            std::cout << "Down arrow key pressed" << std::endl;
            break;
        default:
            break;
    }
}

void handle_keyup(SDL_Keycode key) {
    switch (key) {
        case SDLK_UP:
            std::cout << "Up arrow key released" << std::endl;
            break;
        case SDLK_DOWN:
            std::cout << "Down arrow key released" << std::endl;
            break;
        default:
            break;
    }
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow("SDL OpenGL Example",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SCREEN_WIDTH, SCREEN_HEIGHT,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        std::cerr << "OpenGL context could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                handle_keydown(event.key.keysym.sym);
            } else if (event.type == SDL_KEYUP) {
                handle_keyup(event.key.keysym.sym);
            }
        }

        // OpenGL rendering code here
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // Swap buffers
        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}