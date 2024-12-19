#pragma once 
#ifndef PIANO_H
#define PIANO_H

#include <queue>
#include "keyboard/key.h"
#include "audio_render_stage/audio_render_stage.h"

#define MIDDLE_C 261.63f
#define SEMI_TONE 1.059463f

class Piano {
public:
    Piano(const unsigned int init_pool_size);
    ~Piano();

    void key_down(const float tone, const float gain);
    void key_up(const float tone);

    static const std::unordered_map<unsigned char, float> KEY_TONE_MAPPING;

    AudioRenderStage * get_first_render_stage() {return m_first_render_stage;}
    AudioRenderStage * get_last_render_stage() {return m_last_render_stage;}

private:
    std::queue<std::unique_ptr<PianoKey>> m_key_pool;
    std::unordered_map<float, std::unique_ptr<PianoKey>> m_pressed_keys;
    AudioRenderStage * m_first_render_stage;
    AudioRenderStage * m_last_render_stage;
};


#endif // PIANO