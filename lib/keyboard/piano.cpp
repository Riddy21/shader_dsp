#include <math.h>

#include "keyboard/piano.h"

const std::unordered_map<unsigned char, float> Piano::KEY_TONE_MAPPING = {
    {'a', MIDDLE_C},
    {'w', MIDDLE_C * std::pow(SEMI_TONE, 1)},
    {'s', MIDDLE_C * std::pow(SEMI_TONE, 2)},
    {'e', MIDDLE_C * std::pow(SEMI_TONE, 3)},
    {'d', MIDDLE_C * std::pow(SEMI_TONE, 4)},
    {'f', MIDDLE_C * std::pow(SEMI_TONE, 5)},
    {'t', MIDDLE_C * std::pow(SEMI_TONE, 6)},
    {'g', MIDDLE_C * std::pow(SEMI_TONE, 7)},
    {'y', MIDDLE_C * std::pow(SEMI_TONE, 8)},
    {'h', MIDDLE_C * std::pow(SEMI_TONE, 9)},
    {'u', MIDDLE_C * std::pow(SEMI_TONE, 10)},
    {'j', MIDDLE_C * std::pow(SEMI_TONE, 11)},
    {'k', MIDDLE_C * std::pow(SEMI_TONE, 12)},
};

Piano::Piano(const unsigned int init_pool_size) {
    for (unsigned int i = 0; i < init_pool_size; i++) {
        add_key();
    }
}

Piano::~Piano() {
    // Clean up key pool
    while (!m_key_pool.empty()) {
        m_key_pool.pop();
    }
}

void Piano::add_key() {
    auto key = std::make_unique<PianoKey>('_'); // Placeholder name
    m_key_pool.push(std::move(key));
}

void Piano::key_down(const float tone, const float gain) {
    if (m_key_pool.empty()) {
        // Add a new key
        //TODO: Currently cannot add render stage after initialization, need to enable this in the future
        add_key();
        return;
    }
    auto key = std::move(m_key_pool.front());
    m_key_pool.pop();

    key->set_tone(tone);
    key->set_gain(gain);
    key->key_down();

    // put key in temporary storage
    m_pressed_keys[tone] = std::move(key);
}

void Piano::key_up(const float tone) {
    // Get key from temporary storage and delete
    auto it = m_pressed_keys.find(tone);
    if (it != m_pressed_keys.end()) {
        auto key = std::move(it->second);
        key->key_up();
        m_key_pool.push(std::move(key));
        m_pressed_keys.erase(it);
    }
}
