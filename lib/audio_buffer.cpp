#include <cstdio>
#include <vector>
#include <cstring>
#include <GL/glew.h>
#include <GL/glut.h>

#include "audio_buffer.h"

AudioBuffer::AudioBuffer(unsigned int max_size, unsigned int buffer_size)
    : m_buffer_size(buffer_size), m_max_size(max_size) {
    // Error check
    if (max_size == 0) {
        fprintf(stderr, "Error: Max size must be greater than 0.\n");
        return;
    }

    // Initialize circular queue
    m_circular_queue.resize(max_size);
    for (unsigned int i = 0; i < max_size; i++) {
        m_circular_queue[i] = new float[buffer_size];
    }
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

void AudioBuffer::push(const float * buffer) {
    this->wait();

    m_mutex.lock();
    // Have to copy because don't know if buffer is deallocated
    float * buffer_copy = new float[m_buffer_size];
    memcpy(buffer_copy, buffer, m_buffer_size * sizeof(float));
    m_circular_queue[m_write_index] = buffer_copy; //copy buffer to write_index
    m_mutex.unlock();

    m_write_index = (m_write_index + 1) % m_max_size;

    // Calculate frame rate
    static int frame_count = 0;
    static double previous_time = 0.0;
    static double fps = 0.0;

    frame_count++;
    double current_time = glutGet(GLUT_ELAPSED_TIME) / 1000.0; // Get current time in seconds
    double elapsed_time = current_time - previous_time; // Calculate elapsed time

    if (elapsed_time > 1.0) { // If more than 1 second has elapsed
        fps = frame_count / elapsed_time; // Calculate the frame rate
        frame_count = 0; // Reset the frame count
        previous_time = current_time; // Update the previous time
    }
    //printf("Audio Input Frame rate: %f\n", fps); // Print the frame rate

}

const float * AudioBuffer::pop() {
    this->notify();

    m_mutex.lock();
    const float * buffer = m_circular_queue[m_read_index];
    m_mutex.unlock();

    //if (m_read_index >= m_write_index) {
    //    printf("Num Elements: %d\n", m_circular_queue.size() - m_read_index + m_write_index);
    //} else {
    //    printf("Num Elements: %d\n", m_write_index - m_read_index);
    //}

    // If the num elements gets below 1 don't increment read_index
    if (((m_read_index + 1) % (m_circular_queue.size())) == m_write_index) {
        //printf("WARNING: Audio Queue Underrun!\n");
    } else {
        m_read_index = (m_read_index + 1) % (m_circular_queue.size());
    }

    // Calculate frame rate
    static int frame_count = 0;
    static double previous_time = 0.0;
    static double fps = 0.0;

    frame_count++;
    double current_time = glutGet(GLUT_ELAPSED_TIME) / 1000.0; // Get current time in seconds
    double elapsed_time = current_time - previous_time; // Calculate elapsed time

    if (elapsed_time > 1.0) { // If more than 1 second has elapsed
        fps = frame_count / elapsed_time; // Calculate the frame rate
        frame_count = 0; // Reset the frame count
        previous_time = current_time; // Update the previous time
    }
    printf("Audio Output Frame rate: %f\n", fps); // Print the frame rate

    return buffer;
}

