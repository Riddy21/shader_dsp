#pragma once
#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>

class AudioBuffer {
public:
    AudioBuffer(const unsigned int max_size, const unsigned int buffer_size);
    ~AudioBuffer();
    void push(const float * buffer);
    const float * pop();
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
    void new_notify() {
        std::lock_guard<std::mutex> lock(m_mutex2);
        m_flag = true;
        m_condition.notify_one();
    }
    void new_wait() {
        std::unique_lock<std::mutex> lock(m_mutex2);
        m_condition.wait(lock, [this](){return m_flag;});
        m_flag = false;
    }

private:
    std::vector<const float *> m_circular_queue;
    unsigned int m_read_index = 0;
    unsigned int m_write_index = 0;
    const unsigned int m_buffer_size;
    const unsigned int m_max_size;
    std::mutex m_mutex;
    std::mutex m_mutex2;
    std::condition_variable m_condition;
    bool flag = false;
    bool m_flag = false;
};

#endif // AUDIO_BUFFER_H