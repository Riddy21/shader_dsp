#pragma once 
#ifndef PIANO_H
#define PIANO_H

#include <queue>
#include "keyboard/key.h"

#define MIDDLE_C 261.63f
#define SEMI_TONE 1.059463f

class Piano {
public:
    Piano(const unsigned int init_pool_size);
    ~Piano();

    void key_down(const float tone, const float gain);
    void key_up(const float tone);

    static const std::unordered_map<unsigned char, float> KEY_TONE_MAPPING;

private:
    std::queue<std::unique_ptr<PianoKey>> m_key_pool;
    std::unordered_map<float, std::unique_ptr<PianoKey>> m_pressed_keys;

    void add_key();
};


#endif // PIANO