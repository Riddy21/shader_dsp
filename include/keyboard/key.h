#pragma once
#ifndef KEY_H
#define KEY_H

#include <functional>

#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_core/audio_parameter.h"

class Key {
public:
    Key(const unsigned char name) : name(name) {}

    ~Key() {}

    void key_down() {
        if (m_key_down_callback) {
            m_key_down_callback();
        }
        m_key_pressed = true;
    }
    void key_up() {
        if (m_key_up_callback) {
            m_key_up_callback();
        }
        m_key_pressed = false;
    }

    void set_key_down_callback(std::function<void()> callback) { m_key_down_callback = callback; }
    void set_key_up_callback(std::function<void()> callback) { m_key_up_callback = callback; }

    bool is_pressed() const { return m_key_pressed; }

    const unsigned char name;


private:
    std::function<void()> m_key_down_callback;
    std::function<void()> m_key_up_callback;

    bool m_key_pressed = false;
};

#endif // KEY_H