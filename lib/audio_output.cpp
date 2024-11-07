#include "audio_output.h"

void AudioOutput::update_latency() {
    // Calculate the latency
    static unsigned int frame_count = 0;
    static auto previous_time = std::chrono::high_resolution_clock::now();

    frame_count++;
    auto current_time = std::chrono::high_resolution_clock::now();
    auto elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(current_time - previous_time).count();

    m_latency = (int)(elapsed_time / frame_count);
}