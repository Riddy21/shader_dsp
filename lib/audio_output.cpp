#include "audio_output.h"

void AudioOutput::update_latency() {
    // If its the first frame, set the previous time
    if (m_frame_count == 0) {
        m_previous_time = std::chrono::high_resolution_clock::now();
    }

    m_frame_count++;
    auto current_time = std::chrono::high_resolution_clock::now();
    auto elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(current_time - m_previous_time).count();

    m_latency = (int)(elapsed_time / m_frame_count);
}