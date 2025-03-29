#include <iostream>
#include <unordered_set>

#include "audio_core/audio_renderer.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "keyboard/keyboard.h"
#include "keyboard/piano.h"

Keyboard * Keyboard::instance = nullptr;

Keyboard::Keyboard() : m_piano(10) {

}

Keyboard::~Keyboard() {}

// GLFW key callback function
void Keyboard::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (Piano::KEY_TONE_MAPPING.find(key) != Piano::KEY_TONE_MAPPING.end()) {
            instance->m_piano.key_down(Piano::KEY_TONE_MAPPING.at(key), .2f);
        } else if (instance->m_keys.find(key) != instance->m_keys.end()) {
            instance->m_keys[key]->key_down();
        }
    } else if (action == GLFW_RELEASE) {
        if (Piano::KEY_TONE_MAPPING.find(key) != Piano::KEY_TONE_MAPPING.end()) {
            instance->m_piano.key_up(Piano::KEY_TONE_MAPPING.at(key));
        } else if (instance->m_keys.find(key) != instance->m_keys.end()) {
            instance->m_keys[key]->key_up();
        }
    }
}

void Keyboard::add_key(Key * key) {
    m_keys[key->name] = std::unique_ptr<Key>(key);
}

bool Keyboard::initialize(GLFWwindow* window) {
    glfwSetKeyCallback(window, key_callback);
    return true;
}