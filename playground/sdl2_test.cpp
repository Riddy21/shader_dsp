#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <iostream>
#include <chrono>
#include <thread>

class SDL2Test {
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    Mix_Chunk* testSound;
    bool running;
    int windowWidth;
    int windowHeight;

public:
    SDL2Test() : window(nullptr), renderer(nullptr), testSound(nullptr), 
                  running(false), windowWidth(800), windowHeight(600) {}

    ~SDL2Test() {
        cleanup();
    }

    bool init() {
        // Initialize SDL
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
            std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }

        // Initialize SDL_mixer
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
            std::cerr << "SDL_mixer could not initialize! Mix_Error: " << Mix_GetError() << std::endl;
            return false;
        }

        // Create window
        window = SDL_CreateWindow("SDL2 Test - Audio & Graphics", 
                                 SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                 windowWidth, windowHeight, 
                                 SDL_WINDOW_SHOWN);
        if (!window) {
            std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }

        // Create renderer
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }

        // Generate a simple test sound (440Hz sine wave)
        generateTestSound();

        std::cout << "SDL2 Test initialized successfully!" << std::endl;
        std::cout << "Controls:" << std::endl;
        std::cout << "  SPACE - Play test sound" << std::endl;
        std::cout << "  ESC   - Quit" << std::endl;
        std::cout << "  Mouse - Move colored rectangle" << std::endl;
        std::cout << std::endl;

        return true;
    }

    void generateTestSound() {
        // Create a simple 440Hz sine wave sound
        const int sampleRate = 44100;
        const int duration = 1; // 1 second
        const int samples = sampleRate * duration;
        const int frequency = 440; // A4 note
        
        Uint8* buffer = new Uint8[samples * 2]; // 16-bit stereo
        
        for (int i = 0; i < samples; i++) {
            // Generate sine wave
            Sint16 sample = (Sint16)(32767.0 * sin(2.0 * M_PI * frequency * i / sampleRate));
            
            // Convert to 8-bit stereo
            buffer[i * 2] = sample & 0xFF;         // Left channel
            buffer[i * 2 + 1] = (sample >> 8) & 0xFF; // Right channel
        }
        
        SDL_RWops* rw = SDL_RWFromMem(buffer, samples * 2);
        testSound = Mix_LoadWAV_RW(rw, 1);
        
        delete[] buffer;
        
        if (!testSound) {
            std::cerr << "Failed to generate test sound! Mix_Error: " << Mix_GetError() << std::endl;
        }
    }

    void run() {
        running = true;
        SDL_Event event;
        
        // Animation variables
        int rectX = windowWidth / 2 - 50;
        int rectY = windowHeight / 2 - 50;
        int rectW = 100;
        int rectH = 100;
        int rectSpeedX = 2;
        int rectSpeedY = 2;
        
        auto lastTime = std::chrono::high_resolution_clock::now();
        
        while (running) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            auto deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime);
            lastTime = currentTime;
            
            // Handle events
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                    case SDL_QUIT:
                        running = false;
                        break;
                    case SDL_KEYDOWN:
                        handleKeyPress(event.key.keysym.sym);
                        break;
                    case SDL_MOUSEMOTION:
                        rectX = event.motion.x - rectW / 2;
                        rectY = event.motion.y - rectH / 2;
                        break;
                }
            }
            
            // Update animation
            rectX += rectSpeedX;
            rectY += rectSpeedY;
            
            // Bounce off walls
            if (rectX <= 0 || rectX >= windowWidth - rectW) {
                rectSpeedX = -rectSpeedX;
            }
            if (rectY <= 0 || rectY >= windowHeight - rectH) {
                rectSpeedY = -rectSpeedY;
            }
            
            // Render
            render(rectX, rectY, rectW, rectH);
            
            // Cap frame rate
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }
    }

    void handleKeyPress(SDL_Keycode key) {
        switch (key) {
            case SDLK_SPACE:
                if (testSound) {
                    Mix_PlayChannel(-1, testSound, 0);
                    std::cout << "Playing test sound (440Hz sine wave)" << std::endl;
                }
                break;
            case SDLK_ESCAPE:
                running = false;
                break;
        }
    }

    void render(int rectX, int rectY, int rectW, int rectH) {
        // Clear screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        // Draw animated rectangle
        SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255);
        SDL_Rect rect = {rectX, rectY, rectW, rectH};
        SDL_RenderFillRect(renderer, &rect);
        
        // Draw border
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &rect);
        
        // Present render
        SDL_RenderPresent(renderer);
    }

    void cleanup() {
        if (testSound) {
            Mix_FreeChunk(testSound);
            testSound = nullptr;
        }
        
        if (renderer) {
            SDL_DestroyRenderer(renderer);
            renderer = nullptr;
        }
        
        if (window) {
            SDL_DestroyWindow(window);
            window = nullptr;
        }
        
        Mix_Quit();
        SDL_Quit();
    }
};

int main(int argc, char* argv[]) {
    std::cout << "=== SDL2 C++ Test Program ===" << std::endl;
    std::cout << "Testing SDL2 with PulseAudio integration" << std::endl;
    std::cout << std::endl;
    
    // Check PulseAudio connection
    std::cout << "Checking PulseAudio connection..." << std::endl;
    FILE* pulseCheck = popen("pactl info 2>/dev/null", "r");
    if (pulseCheck) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pulseCheck)) {
            std::cout << "✓ PulseAudio connection successful" << std::endl;
        } else {
            std::cout << "⚠ Warning: PulseAudio connection may not be working" << std::endl;
        }
        pclose(pulseCheck);
    }
    
    std::cout << std::endl;
    
    SDL2Test test;
    
    if (!test.init()) {
        std::cerr << "Failed to initialize SDL2 test!" << std::endl;
        return 1;
    }
    
    test.run();
    
    std::cout << "SDL2 test completed successfully!" << std::endl;
    return 0;
} 