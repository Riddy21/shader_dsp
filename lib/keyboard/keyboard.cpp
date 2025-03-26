#include <iostream>
#include <unordered_set>
#include <X11/Xlib.h>

#include "audio_core/audio_renderer.h"
#include "audio_render_stage/audio_generator_render_stage.h"
#include "keyboard/keyboard.h"

Keyboard * Keyboard::instance = nullptr;

Keyboard::Keyboard() {

}

Keyboard::~Keyboard() {
}

bool Keyboard::terminate() {
    // Enable key repeat
    Display *display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        std::cerr << "Unable to open X display" << std::endl;
        return false;
    }

    int result = XAutoRepeatOn(display);
    printf("Enabled key repeat\n");

    XCloseDisplay(display);

    // clean up the keys
    m_keys.clear();

    // clean up the instance
    if (instance) {
        delete instance;
        instance = nullptr;
    }
    return true;
}

void Keyboard::key_down_callback(unsigned char key, int x, int y) {
    if (instance->m_keys.find(key) != instance->m_keys.end()) {
        instance->m_keys[key]->key_down();
        printf("Key down: %c\n", key);
    }
}

void Keyboard::key_up_callback(unsigned char key, int x, int y) {
    if (instance->m_keys.find(key) != instance->m_keys.end()) {
        instance->m_keys[key]->key_up();
        printf("Key up: %c\n", key);
    }
}

void Keyboard::add_key(Key * key) {
    m_keys[key->name] = std::unique_ptr<Key>(key);
}

bool Keyboard::initialize() {
    glutKeyboardFunc(key_down_callback);
    glutKeyboardUpFunc(key_up_callback);

    // Disable key repeat
    Display *display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        std::cerr << "Unable to open X display" << std::endl;
        return false;
    }

    int result = XAutoRepeatOff(display);
    printf("Disabled key repeat\n");

    XCloseDisplay(display);

    return true;
}