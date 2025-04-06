#include <iostream>
#include <unordered_set>
#include <SDL2/SDL.h>
#include <thread>
#include <atomic>

#include "audio_core/audio_renderer.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "keyboard/keyboard.h"

Keyboard * Keyboard::instance = nullptr;

Keyboard::Keyboard() : m_running(false) {

}

Keyboard::~Keyboard() {
    terminate();
}

bool Keyboard::terminate() {
    // Stop the game loop
    m_running = false;
    if (m_game_loop_thread.joinable()) {
        m_game_loop_thread.join();
    }

    // Clean up the keys
    m_keys.clear();

    // Clean up the instance
    if (instance) {
        delete instance;
        instance = nullptr;
    }

    return true;
}

bool Keyboard::initialize() {
    // FIXME: Keyboard loop seems to freeze and cause dropped key presses
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Unable to initialize SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    m_running = true;

    // Start the game loop in a separate thread
    m_game_loop_thread = std::thread(&Keyboard::game_loop, this);

    return true;
}

void Keyboard::game_loop() {
    while (m_running) {
        process_events();
    }
}

void Keyboard::process_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        process_event(event);
    }
}

void Keyboard::process_event(const SDL_Event &event) {
    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        SDL_Keycode key = event.key.keysym.sym;
        char ascii_key = '\0';

        if (key >= SDLK_a && key <= SDLK_z) {
            ascii_key = static_cast<char>(key); // Lowercase letters
        } else if (key >= SDLK_SPACE && key <= SDLK_z) {
            ascii_key = static_cast<char>(key);
        }

        if (ascii_key != '\0') {
            auto it = m_keys.find(ascii_key);
            if (it != m_keys.end()) {
                if (event.type == SDL_KEYDOWN) {
                    if (!it->second->is_pressed()) { // Check if the key is already pressed
                        it->second->key_down();
                    }
                } else if (event.type == SDL_KEYUP) {
                    it->second->key_up();
                }
            }
        }
    }
}

void Keyboard::add_key(Key * key) {
    m_keys[key->name] = std::unique_ptr<Key>(key);
}