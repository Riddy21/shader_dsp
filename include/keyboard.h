#pragma once
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <unordered_map>

#include "key.h"
#include "audio_generator_render_stage.h"

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
    void add_key(Key * key);
    Key * get_key(const unsigned char key) { return m_keys[key].get(); }

private:
    Keyboard() {}
    ~Keyboard();

    static void key_down_callback(unsigned char key, int x, int y);
    static void key_up_callback(unsigned char key, int x, int y);

    static Keyboard * instance;

    unsigned int m_num_octaves;
    AudioRenderer & m_audio_renderer = AudioRenderer::get_instance();
    std::unordered_map<unsigned char, std::unique_ptr<Key>> m_keys;

};

#endif // KEYBOARD_H