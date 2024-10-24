#pragma once
#ifndef KEY_H
#define KEY_H

#include "audio_generator_render_stage.h"
#include "audio_parameter.h"

class Key {
public:
    Key(const unsigned char name) : name(name) {}
    ~Key() {}

    virtual void key_down() = 0;
    virtual void key_up() = 0;

    const unsigned char name;
};

class PianoKey : public Key {
public:
    PianoKey(const unsigned char key, const char * audio_file_path);
    void key_down() override;
    void key_up() override;
    void set_gain(const float gain) { m_gain = gain; }
    void set_tone(const float tone) { m_tone = tone; }

private:
    AudioGeneratorRenderStage * m_audio_generator;
    AudioParameter * m_gain_param;
    AudioParameter * m_tone_param;
    AudioParameter * m_play_param;
    AudioParameter * m_time_param;
    float m_gain;
    float m_tone;
};

#endif // KEY_H