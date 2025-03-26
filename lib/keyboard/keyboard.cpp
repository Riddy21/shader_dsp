#include <iostream>
#include <unordered_set>

#include "audio_core/audio_renderer.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "keyboard/keyboard.h"

Keyboard * Keyboard::instance = nullptr;

Keyboard::Keyboard() {

}

Keyboard::~Keyboard() {}

void Keyboard::key_down_callback(unsigned char key, int x, int y) {
    if (instance->m_keys.find(key) != instance->m_keys.end()) {
        instance->m_keys[key]->key_down();
    }
}

void Keyboard::key_up_callback(unsigned char key, int x, int y) {
    if (instance->m_keys.find(key) != instance->m_keys.end()) {
        instance->m_keys[key]->key_up();
    }
}

void Keyboard::add_key(Key * key) {
    m_keys[key->name] = std::unique_ptr<Key>(key);
}

bool Keyboard::initialize() {
    glutKeyboardFunc(key_down_callback);
    glutKeyboardUpFunc(key_up_callback);

    return true;
}