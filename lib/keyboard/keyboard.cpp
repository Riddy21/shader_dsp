#include <iostream>
#include <unordered_set>
#include <SDL2/SDL.h>
#include <thread>
#include <atomic>

#include "audio_core/audio_renderer.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "keyboard/keyboard.h"

// Removed: Keyboard * Keyboard::instance = nullptr;

Keyboard::Keyboard() {
}

Keyboard::~Keyboard() {
    m_keys.clear();
}

bool Keyboard::initialize() {
    // Only set running flag, no SDL_Init or thread
    return true;
}

// Remove game_loop and process_events, as event loop will call handle_event

void Keyboard::handle_event(const SDL_Event &event) {
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