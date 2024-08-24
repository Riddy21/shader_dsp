#pragma once
#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <vector>
#include <mutex>

class AudioBuffer {
public:
    AudioBuffer(const unsigned int max_size);
    ~AudioBuffer();
    void push(const std::vector<float> & buffer);
    const std::vector<float> & pop();
    void clear();

private:
    std::vector<std::vector<float>> circular_queue;
    unsigned int max_size;
    unsigned int read_index = 0;
    unsigned int write_index = 0;
    std::mutex mutex;
};

#endif // AUDIO_BUFFER_H