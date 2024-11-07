#pragma once
#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <vector>
#include <mutex>
#include <thread>

/**
 * @class AudioBuffer
 * @brief The AudioBuffer class is responsible for managing a circular buffer for audio data.
 */
class AudioBuffer {
public:
    /**
     * @brief Constructs an AudioBuffer object.
     * 
     * @param max_size The maximum number of buffers in the circular queue.
     * @param buffer_size The size of each buffer.
     */
    AudioBuffer(const unsigned int max_size, const unsigned int buffer_size);

    /**
     * @brief Destroys the AudioBuffer object.
     */
    ~AudioBuffer();

    /**
     * @brief Pushes a buffer into the circular queue.
     * 
     * @param buffer The buffer to push.
     * @param quiet If true, suppresses warnings.
     */
    void push(const float * buffer, const bool quiet = false);

    /**
     * @brief Pops a buffer from the circular queue.
     * 
     * @param quiet If true, suppresses warnings.
     * @return The buffer popped from the queue.
     */
    const float * pop(const bool quiet = false);

    /**
     * @brief Updates the buffer at the current write index.
     * 
     * @param buffer The buffer to update.
     */
    void update(const float * buffer);

    /**
     * @brief Increments the write index of the circular queue.
     */
    void increment_write_index();

    /**
     * @brief Clears the circular queue.
     */
    void clear();

    /**
     * @brief Returns the size of the circular queue.
     * 
     * @return The size of the circular queue.
     */
    unsigned int get_size() { return m_circular_queue.size(); }

    /**
     * @brief Returns the maximum size of the circular queue.
     * 
     * @return The maximum size of the circular queue.
     */
    unsigned int get_max_size() { return m_max_size; }

private:
    std::vector<const float *> m_circular_queue; ///< The circular queue of buffers.
    unsigned int m_read_index = 0; ///< The current read index.
    unsigned int m_write_index = 0; ///< The current write index.
    const unsigned int m_buffer_size; ///< The size of each buffer.
    const unsigned int m_max_size; ///< The maximum number of buffers in the circular queue.
    unsigned int m_num_elements = 0; ///< The number of elements in the circular queue.
};

#endif // AUDIO_BUFFER_H