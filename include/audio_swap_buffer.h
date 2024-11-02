#pragma once
#ifndef AUDIO_SWAP_BUFFER_H
#define AUDIO_SWAP_BUFFER_H

#include <vector>
#include <mutex>
#include <thread>

class AudioSwapBuffer {
public:
    AudioSwapBuffer(const unsigned int buffer_size);
    ~AudioSwapBuffer();
    void clear();
    void swap_buffers();
    void write_buffer(const float * buffer, const bool quiet = false);
    const float * read_buffer();
    void notify() { m_flag = true;}
    void wait() {
        while (!m_flag) {
            std::this_thread::yield();
        }
        m_flag = false;
    }

private:
    bool m_flag = false;
    std::mutex m_mutex;
    float * m_buffer_a;
    float * m_buffer_b;
    float * m_read_buffer;
    float * m_write_buffer;
    const int m_buffer_size;
};

#endif // AUDIO_SWAP_BUFFER_H
