#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <iostream>

// Function to render text
SDL_Texture* render_text(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
    if (!surface) {
        std::cerr << "Failed to create text surface: " << TTF_GetError() << std::endl;
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

// Function to render an image
SDL_Texture* load_image(SDL_Renderer* renderer, const std::string& path) {
    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
        std::cerr << "Failed to load image: " << IMG_GetError() << std::endl;
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

// Pong game function (simplified for demonstration)
void pong_game(SDL_Window* window, SDL_Renderer* renderer) {
    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        // Clear screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Draw a simple placeholder for the Pong game
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect ball = {390, 290, 20, 20};
        SDL_RenderFillRect(renderer, &ball);

        SDL_RenderPresent(renderer);
    }
}

int main() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Initialize SDL_ttf
    if (TTF_Init() != 0) {
        std::cerr << "Failed to initialize SDL_ttf: " << TTF_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    // Initialize SDL_image
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        std::cerr << "Failed to initialize SDL_image: " << IMG_GetError() << std::endl;
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    // Create SDL window
    SDL_Window* window = SDL_CreateWindow("Pong Menu", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << std::endl;
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    // Create SDL renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "Failed to create SDL renderer: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    // Load font
    TTF_Font* font = TTF_OpenFont("/home/ridvan/Downloads/love-days-love-font/LoveDays-2v7Oe.ttf", 48); // Replace with the path to your font file
    if (!font) {
        std::cerr << "Failed to load font: " << TTF_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    // Load background image
    SDL_Texture* background = load_image(renderer, "/home/ridvan/Downloads/DSC_6016.JPEG"); // Replace with the path to your image file
    if (!background) {
        TTF_CloseFont(font);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    // Render menu text
    SDL_Color white = {255, 255, 255, 255};
    SDL_Texture* title = render_text(renderer, font, "Pong Game", white);
    SDL_Texture* start = render_text(renderer, font, "Press Enter to Start", white);

    // Main menu loop
    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RETURN) {
                pong_game(window, renderer); // Start the Pong game
            }
        }

        // Clear screen
        SDL_RenderClear(renderer);

        // Render background
        SDL_RenderCopy(renderer, background, nullptr, nullptr);

        // Render title
        int title_w, title_h;
        SDL_QueryTexture(title, nullptr, nullptr, &title_w, &title_h);
        SDL_Rect title_rect = {400 - title_w / 2, 100, title_w, title_h};
        SDL_RenderCopy(renderer, title, nullptr, &title_rect);

        // Render start text
        int start_w, start_h;
        SDL_QueryTexture(start, nullptr, nullptr, &start_w, &start_h);
        SDL_Rect start_rect = {400 - start_w / 2, 300, start_w, start_h};
        SDL_RenderCopy(renderer, start, nullptr, &start_rect);

        SDL_RenderPresent(renderer);
    }

    // Cleanup
    SDL_DestroyTexture(title);
    SDL_DestroyTexture(start);
    SDL_DestroyTexture(background);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    return 0;
}
