#pragma once
#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <vector>
#include <mutex>
#include <thread>

class AudioBuffer {
public:
    AudioBuffer(const unsigned int max_size, const unsigned int buffer_size);
    ~AudioBuffer();
    void push(const float * buffer, const bool quiet = false);
    const float * pop(const bool quiet = false);
    void update(const float * buffer);
    void increment_write_index();
    void clear();
    unsigned int get_size() { return m_circular_queue.size(); }
    unsigned int get_max_size() { return m_max_size; }
    void notify() { flag = true;}
    void wait() {
        while (!flag) {
            std::this_thread::yield();
        }
        flag = false;
    }

private:
    std::vector<const float *> m_circular_queue;
    unsigned int m_read_index = 0;
    unsigned int m_write_index = 0;
    const unsigned int m_buffer_size;
    const unsigned int m_max_size;
    unsigned int m_num_elements = 0;
    std::mutex m_mutex;
    bool flag = false;
};

#endif // AUDIO_BUFFER_H