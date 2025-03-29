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

void Keyboard::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    // Translate GLFW key to ASCII if possible
    // TODO: Keep these as numbers in the future
    char ascii_key = '\0';
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
        ascii_key = static_cast<char>(key + 32); // Convert to lowercase
    } else if (key >= GLFW_KEY_SPACE && key <= GLFW_KEY_Z) {
        ascii_key = static_cast<char>(key);
    }

    if (ascii_key != '\0') {
        if (action == GLFW_PRESS) {
            if (instance->m_keys.find(ascii_key) != instance->m_keys.end()) {
                instance->m_keys[ascii_key]->key_down();
                printf("Key down: %c\n", ascii_key);
            }
        } else if (action == GLFW_RELEASE) {
            if (instance->m_keys.find(ascii_key) != instance->m_keys.end()) {
                instance->m_keys[ascii_key]->key_up();
                printf("Key up: %c\n", ascii_key);
            }
        }
    }
}

void Keyboard::add_key(Key * key) {
    m_keys[key->name] = std::unique_ptr<Key>(key);
}

bool Keyboard::initialize(GLFWwindow* window) {
    glfwSetKeyCallback(window, key_callback); // Set GLFW key callback

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