#include <fstream>
#include <thread>
#include <cstring>

#include "audio_wav.h"
#include "audio_file_output.h"
#include "audio_renderer.h"

AudioFileOutput::~AudioFileOutput() {
}

bool AudioFileOutput::open() {
    // Open file from m_filename
    m_file = std::ofstream(m_filename, std::ios::binary);
    // Check that file is open
    if (!m_file.is_open()) {
        fprintf(stderr, "Error: File not open.\n");
        return false;
    }
    return true;
}

bool AudioFileOutput::close() {
    // Complete the audio file header
    m_header.overall_size = (size_t)m_file.tellp() - 8;
    m_header.data_size = (size_t)m_file.tellp() - sizeof(WAVHeader);

    m_file.seekp(0);
    m_file.write((char *) &m_header, sizeof(WAVHeader));

    // Close file
    m_file.close();

    // Check that file is closed
    if (m_file.is_open()) {
        fprintf(stderr, "Error: File not closed.\n");
        return false;
    }
    return true;
}

bool AudioFileOutput::start() {
    // Start writing audio data to the file
    if (!m_file.is_open()) {
        fprintf(stderr, "Error: File not open.\n");
        return false;
    }

    // Write the audio file header
    m_header.length_of_fmt = 16;
    m_header.format_type = 1;
    m_header.channels = m_channels;
    m_header.sample_rate = m_sample_rate;
    m_header.bits_per_sample = 16;
    m_header.byte_rate = m_header.sample_rate * m_header.channels * m_header.bits_per_sample / 8;
    m_header.block_align = m_header.channels * m_header.bits_per_sample / 8;
    m_header.data_size = 0;

    m_file.write((char *) &m_header, sizeof(WAVHeader));

    m_is_running = true;

    // Start a thread to write audio data to the file
    std::thread t1(write_audio_callback, this);
    t1.detach();

    return true;
}

void AudioFileOutput::write_audio_callback(AudioFileOutput* audio_file_output) {
    // Write audio data to the file
    if (!audio_file_output->m_file.is_open()) {
        fprintf(stderr, "Error: File not open.\n");
        return;
    }
    auto & audio_renderer = AudioRenderer::get_instance();
    while (audio_file_output->m_is_running) {
        // Write audio data to the file
        audio_file_output->m_audio_buffer_link->increment_write_index();
        auto audio_buffer = audio_file_output->m_audio_buffer_link->pop();
        audio_renderer.increment_frame_count();

        for (unsigned i = 0; i < audio_file_output->m_frames_per_buffer*audio_file_output->m_channels; i++) {
            // convert float to int16_t
            int16_t sample = (int16_t)(audio_buffer[i] * 32760.0f);
            audio_file_output->m_file.write((char *) &sample, sizeof(int16_t));
        }

        // Wait for a short time
        std::this_thread::sleep_until(std::chrono::steady_clock::now() + std::chrono::milliseconds((int)(1000.0f/((double)audio_file_output->m_sample_rate/(double)audio_file_output->m_frames_per_buffer))));
    }
}

bool AudioFileOutput::stop() {
    if (!m_file.is_open()) {
        fprintf(stderr, "Error: File not open.\n");
        return false;
    }
    m_is_running = false;
    return true;
}