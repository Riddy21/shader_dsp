#pragma once
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <unordered_map>
#include <SDL2/SDL.h>
#include <thread>
#include <atomic>

#include "keyboard/key.h"
#include "audio_core/audio_render_stage.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "engine/event_loop.h"

class Keyboard : public IEventLoopItem {
public:
    Keyboard();
    ~Keyboard();

    Keyboard(const Keyboard&) = delete;
    void operator=(const Keyboard&) = delete;

    // IEventLoopItem interface
    bool is_ready() override { return false; }
    bool handle_event(const SDL_Event &event) override;


    void add_key(Key * key);
    Key * get_key(const unsigned char key) { return m_keys[key].get(); }

private:
    std::unordered_map<unsigned char, std::unique_ptr<Key>> m_keys;
};

#endif // KEYBOARD_H