#pragma once
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <unordered_map>
#include <SDL2/SDL.h>
#include <thread>
#include <atomic>

#include "keyboard/key.h"
#include "audio_render_stage/audio_render_stage.h"
#include "audio_render_stage/audio_generator_render_stage.h"

class Keyboard {
public:
    static Keyboard & get_instance() {
        if (!instance) {
            instance = new Keyboard();
        }
        return *instance;
    }

    Keyboard(Keyboard const&) = delete;
    void operator=(Keyboard const&) = delete;

    bool initialize(); // Starts the game loop on a thread
    bool terminate();
    void add_key(Key * key);
    Key * get_key(const unsigned char key) { return m_keys[key].get(); }

    void process_events(); // Processes SDL events
    void process_event(const SDL_Event &event);

private:
    Keyboard();
    ~Keyboard();

    void game_loop(); // Game loop running on a separate thread

    static Keyboard * instance;

    std::thread m_game_loop_thread; // Thread for the game loop
    std::atomic<bool> m_running; // Flag to control the game loop
    AudioRenderer & m_audio_renderer = AudioRenderer::get_instance();
    std::unordered_map<unsigned char, std::unique_ptr<Key>> m_keys;
};

#endif // KEYBOARD_H