#pragma once
#ifndef KEY_H
#define KEY_H

#include <functional>

#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_parameter/audio_parameter.h"

class Key {
public:
    Key(const unsigned char name) : name(name) {}

    ~Key() {}

    void key_down() {
        if (m_key_down_callback) {
            m_key_down_callback();
        }
    }
    void key_up() {
        if (m_key_up_callback) {
            m_key_up_callback();
        }
    }

    void set_key_down_callback(std::function<void()> callback) { m_key_down_callback = callback; }
    void set_key_up_callback(std::function<void()> callback) { m_key_up_callback = callback; }

    const unsigned char name;


private:
    std::function<void()> m_key_down_callback;
    std::function<void()> m_key_up_callback;
};

class PianoKey : public Key {
public:
    PianoKey(const unsigned char key);

    void set_gain(const float gain);
    void set_tone(const float tone);

    // Getters
    AudioGeneratorRenderStage * get_render_stage() {return m_audio_generator;}

private:
    AudioGeneratorRenderStage * m_audio_generator;
    AudioParameter * m_gain_param;
    AudioParameter * m_tone_param;
    AudioParameter * m_play_position_param;
    AudioParameter * m_stop_position_param;
    AudioParameter * m_time_param;
    AudioParameter * m_play_param;
    float m_gain;
    float m_tone;
};

#endif // KEY_H