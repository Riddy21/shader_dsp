#include <cstdio>
#include <vector>
#include <cstring>

#include "audio_buffer.h"

AudioBuffer::AudioBuffer(const unsigned int max_size) {
    // Error check
    if (max_size == 0) {
        fprintf(stderr, "Error: Max size must be greater than 0.\n");
        return;
    }
    this->m_max_size = max_size;
    m_circular_queue.reserve(max_size);
}

AudioBuffer::~AudioBuffer() {
    clear();
}

void AudioBuffer::clear() {
    for (const float * buffer : m_circular_queue) {
        delete[] buffer;
    }
    m_circular_queue.clear();
}

void AudioBuffer::push(const float * buffer, const unsigned int buffer_size) {
    m_mutex.lock();
    // Have to copy because don't know if buffer is deallocated
    float * buffer_copy = new float[buffer_size];
    memcpy(buffer_copy, buffer, buffer_size * sizeof(float));

    if (m_circular_queue.size() == m_max_size) {
        m_circular_queue[m_write_index] = buffer_copy; //copy buffer to write_index
    } else {
        m_circular_queue.push_back(buffer_copy);
    }
    
    m_mutex.unlock();
    m_write_index = (m_write_index + 1) % m_max_size;
}

const float * AudioBuffer::pop() {
    m_mutex.lock();
    const float * buffer = m_circular_queue[m_read_index];
    m_mutex.unlock();

    //if (read_index >= write_index) {
    //    printf("Num Elements: %d\n", circular_queue.size() - read_index + write_index);
    //} else {
    //    printf("Num Elements: %d\n", write_index - read_index);
    //}

    // If the num elements gets below 1 don't increment read_index
    if (m_read_index == m_write_index) {
        printf("WARNING: Audio Queue Underrun!\n");
    } else {
        m_read_index = (m_read_index + 1) % (m_circular_queue.size());
    }

    return buffer;
}

