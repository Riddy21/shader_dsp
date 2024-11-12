#include "audio_buffers/audio_swap_buffer.h"

AudioSwapBuffer::AudioSwapBuffer(const unsigned int buffer_size)
    : m_buffer_size(buffer_size),
      m_buffer_a(new float[buffer_size]),
      m_buffer_b(new float[buffer_size]),
      m_read_buffer(m_buffer_a),
      m_write_buffer(m_buffer_b) {
}

AudioSwapBuffer::~AudioSwapBuffer() {
    delete[] m_buffer_a;
    delete[] m_buffer_b;
}

void AudioSwapBuffer::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::fill(m_buffer_a, m_buffer_a + m_buffer_size, 0.0f);
    std::fill(m_buffer_b, m_buffer_b + m_buffer_size, 0.0f);
}

void AudioSwapBuffer::swap_buffers() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::swap(m_read_buffer, m_write_buffer);
}

void AudioSwapBuffer::write_buffer(const float * buffer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::copy(buffer, buffer + m_buffer_size, m_write_buffer);
}

const float * AudioSwapBuffer::read_buffer() {
    return m_read_buffer;
}
