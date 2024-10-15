#pragma once
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <vector>
#include <audio_generator_render_stage.h>

class Key {
public:
    Key(const unsigned char key) : m_key(key) {}
    ~Key() {}

    virtual void key_down() = 0;
    virtual void key_up() = 0;
private:
    unsigned char m_key;
};

class PianoKey : public Key {
public:
    PianoKey(const unsigned char key, const char * audio_file_path);
    void key_down() override;
    void key_up() override;

private:
    AudioGeneratorRenderStage * m_audio_generator;
    float m_gain;
    float m_tone;
};

class Keyboard {
public:
    Keyboard();
    ~Keyboard();

    void add_key(std::unique_ptr<Key> key);

private:
    unsigned int m_num_octaves;
    std::vector<std::unique_ptr<Key>> m_keys;
};

#endif // KEYBOARD_H