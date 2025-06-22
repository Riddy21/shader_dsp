#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <vector>
#include <cmath>
#include <string>

// Function to generate a sine wave
std::vector<float> generate_sine_wave(size_t num_samples, float frequency, float amplitude) {
    std::vector<float> wave_data(num_samples);
    for (size_t i = 0; i < num_samples; ++i) {
        wave_data[i] = amplitude * std::sin(2.0f * M_PI * frequency * i / num_samples);
    }
    return wave_data;
}

// Function to render the sound wave as the background
void render_wave(const std::vector<float>& wave_data) {
    glBegin(GL_LINE_STRIP);
    glColor3f(0.0f, 1.0f, 0.0f); // Green color for the wave
    for (size_t i = 0; i < wave_data.size(); ++i) {
        float x = static_cast<float>(i) / wave_data.size() * 2.0f - 1.0f; // Normalize x to [-1, 1]
        float y = wave_data[i]; // Use the wave data for y
        glVertex2f(x, y);
    }
    glEnd();
}

// Function to render text on the screen using SDL2
void render_text(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color, TTF_Font* font) {
    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_Rect dst_rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, nullptr, &dst_rect);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

int main() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        return -1;
    }

    // Initialize SDL_ttf for text rendering
    if (TTF_Init() != 0) {
        SDL_Quit();
        return -1;
    }

    // Set OpenGL attributes for version 4.1
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // Create SDL window
    SDL_Window* window = SDL_CreateWindow("Sound Wave GUI", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    // Create OpenGL context
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Create SDL renderer for text rendering
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    // Load font for text rendering
    TTF_Font* font = TTF_OpenFont("/home/ridvan/Downloads/love-days-love-font/LoveDays-2v7Oe.ttf", 24); // Replace with the path to your font file
    if (!font) {
        SDL_DestroyRenderer(renderer);
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    // Generate test wave data
    const size_t num_samples = 512;
    auto wave_data = generate_sine_wave(num_samples, 2.0f, 1.0f);

    // Main loop
    bool running = true;
    SDL_Event event;
    while (running) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        // Clear the screen
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Dark background
        glClear(GL_COLOR_BUFFER_BIT);

        // Set up the viewport and projection
        glViewport(0, 0, 800, 600);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0); // Orthographic projection
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Render the sound wave
        render_wave(wave_data);

        // Render text
        SDL_Color text_color = {255, 255, 255, 255}; // White color
        render_text(renderer, "Sound Wave Visualization", 10, 10, text_color, font);

        // Swap buffers
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
