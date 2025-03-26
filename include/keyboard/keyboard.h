#pragma once
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <unordered_map>

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

    bool initialize();
    bool terminate();
    void add_key(Key * key);
    Key * get_key(const unsigned char key) { return m_keys[key].get(); }

    static void key_down_callback(unsigned char key, int x, int y);
    static void key_up_callback(unsigned char key, int x, int y);

private:
    Keyboard();
    ~Keyboard();

    static Keyboard * instance;

    AudioRenderer & m_audio_renderer = AudioRenderer::get_instance();
    std::unordered_map<unsigned char, std::unique_ptr<Key>> m_keys;
};

#endif // KEYBOARD_H