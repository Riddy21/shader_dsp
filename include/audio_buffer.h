#pragma once
#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <vector>
#include <mutex>

class AudioBuffer {
public:
    AudioBuffer(const unsigned int max_size);
    ~AudioBuffer();
    void push(const float * buffer, const unsigned int buffer_size);
    const float * pop();
    void clear();
    unsigned int get_size() { return m_circular_queue.size(); }

private:
    std::vector<const float *> m_circular_queue;
    unsigned int m_max_size;
    unsigned int m_read_index = 0;
    unsigned int m_write_index = 0;
    std::mutex m_mutex;
};

#endif // AUDIO_BUFFER_H