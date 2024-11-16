#include <iostream>
#include <unordered_set>

#include "audio_core/audio_renderer.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "keyboard/keyboard.h"

Keyboard * Keyboard::instance = nullptr;

Keyboard::~Keyboard() {
    m_keys.clear();
}

void Keyboard::add_key(Key * key) {
    if (m_keys.find(key->name) != m_keys.end()) {
        std::cerr << "Key already exists." << std::endl;
        return;
    }
    m_keys[key->name] = std::unique_ptr<Key>(key);
}

void Keyboard::key_down_callback(unsigned char key, int x, int y) {
    for (auto & kv : instance->m_keys) {
        if (kv.first == key) {
            kv.second->key_down();
        }
    }
}

void Keyboard::key_up_callback(unsigned char key, int x, int y) {
    for (auto & kv : instance->m_keys) {
        if (kv.first == key) {
            kv.second->key_up();
        }
    }
}

bool Keyboard::initialize() {
    glutKeyboardFunc(key_down_callback);
    glutKeyboardUpFunc(key_up_callback);

    return true;
}