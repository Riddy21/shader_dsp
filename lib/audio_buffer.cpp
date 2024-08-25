#include <cstdio>
#include <vector>

#include "audio_buffer.h"

AudioBuffer::AudioBuffer(const unsigned int max_size) {
    // Error check
    if (max_size == 0) {
        fprintf(stderr, "Error: Max size must be greater than 0.\n");
        return;
    }
    this->max_size = max_size;
    circular_queue.reserve(max_size);
}

AudioBuffer::~AudioBuffer() {
    clear();
}

void AudioBuffer::clear() {
    for (auto & buffer : circular_queue) {
        buffer.clear();
    }
    circular_queue.clear();
}

void AudioBuffer::push(const std::vector<float> & buffer) {
    mutex.lock();
    if (circular_queue.size() == max_size) {
        circular_queue[write_index] = buffer; //copy buffer to write_index
    } else {
        circular_queue.push_back(buffer);
    }
    
    mutex.unlock();
    write_index = (write_index + 1) % max_size;
}

const std::vector<float> & AudioBuffer::pop() {
    mutex.lock();
    const std::vector<float> & buffer = circular_queue[read_index];

    mutex.unlock();
    read_index = (read_index + 1) % circular_queue.size();
    //if (read_index > write_index) {
    //    printf("Num Elements: %d\n", circular_queue.size() - read_index + write_index);
    //} else {
    //    printf("Num Elements: %d\n", write_index - read_index);
    //}
    return buffer;
}

