#include <fstream>
#include <thread>
#include <cstring>

#include "audio_wav.h"
#include "audio_file_output.h"

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

    return true;
}

void AudioFileOutput::push(const float * data) {
    // Write audio data to the file
    if (!m_file.is_open()) {
        fprintf(stderr, "Error: File not open.\n");
        return;
    }

    if (!m_is_running) {
        return;
    }

    for (unsigned i = 0; i < m_frames_per_buffer*m_channels; i++) {
        // convert float to int16_t
        int16_t sample = (int16_t)(data[i] * 32760.0f);
        m_file.write((char *) &sample, sizeof(int16_t));
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

bool AudioFileOutput::is_ready() {
    // Audio is only ready for writing once every
    static auto last_check = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - last_check).count();

    if (duration < 1000000 * m_frames_per_buffer / m_sample_rate) {
        return false;
    }

    last_check = now;

    // Check if the audio file is ready for writing
    if (!m_file.is_open()) {
        return false;
    } else if (!m_is_running) {
        return false;
    } else {
        return true;
    }
}

AudioFileOutput::~AudioFileOutput() {
    // Close the file
    close();
}