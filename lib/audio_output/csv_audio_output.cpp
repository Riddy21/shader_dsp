#include "audio_output/csv_audio_output.h"
#include <iostream>
#include <chrono>

CSVAudioOutput::CSVAudioOutput(unsigned int frames_per_buffer, unsigned int sample_rate, unsigned int channels, const std::string& filename)
    : AudioOutput(frames_per_buffer, sample_rate, channels),
      m_filename(filename),
      m_is_running(false) {
}

CSVAudioOutput::~CSVAudioOutput() {
    if (m_is_running) {
        stop();
    }
    close();
    printf("CSVAudioOutput Destroyed\n");
}

bool CSVAudioOutput::open() {
    // Open CSV file for writing
    m_csv_file.open(m_filename);
    if (!m_csv_file.is_open()) {
        std::cerr << "Failed to open CSV file '" << m_filename << "' for writing" << std::endl;
        return false;
    }
    
    // Write CSV header
    m_csv_file << "sample_index,time_seconds";
    for (unsigned int ch = 0; ch < m_channels; ++ch) {
        m_csv_file << ",channel_" << ch;
    }
    m_csv_file << std::endl;
    
    std::cout << "Opened CSV file: " << m_filename << std::endl;
    return true;
}

bool CSVAudioOutput::start() {
    if (!m_csv_file.is_open()) {
        std::cerr << "Error: CSV file not open." << std::endl;
        return false;
    }
    
    m_audio_buffer.clear();
    m_is_running = true;
    
    std::cout << "Started recording audio to CSV file..." << std::endl;
    return true;
}

bool CSVAudioOutput::stop() {
    if (!m_csv_file.is_open()) {
        std::cerr << "Error: CSV file not open." << std::endl;
        return false;
    }
    
    m_is_running = false;
    std::cout << "Stopped recording audio to CSV file." << std::endl;
    return true;
}

bool CSVAudioOutput::close() {
    if (!m_csv_file.is_open()) {
        return true; // Already closed
    }
    
    // Write all buffered data to CSV
    // m_audio_buffer contains interleaved data: [sample0_ch0, sample0_ch1, sample1_ch0, sample1_ch1, ...]
    size_t total_samples = m_audio_buffer.size() / m_channels;
    
    for (size_t sample = 0; sample < total_samples; ++sample) {
        double time_seconds = static_cast<double>(sample) / m_sample_rate;
        
        // Write sample index and time
        m_csv_file << sample << "," << time_seconds;
        
        // Write all channel data for this sample
        for (unsigned int ch = 0; ch < m_channels; ++ch) {
            size_t buffer_index = sample * m_channels + ch;
            if (buffer_index < m_audio_buffer.size()) {
                m_csv_file << "," << m_audio_buffer[buffer_index];
            } else {
                m_csv_file << ",0.0";  // Pad with zero if missing
            }
        }
        
        m_csv_file << std::endl;
    }
    
    m_csv_file.close();
    m_audio_buffer.clear();
    
    if (m_csv_file.is_open()) {
        std::cerr << "Error: CSV file not closed." << std::endl;
        return false;
    }
    
    std::cout << "Closed CSV file. Audio data saved to " << m_filename << std::endl;
    return true;
}

bool CSVAudioOutput::is_ready() {
    // Audio is only ready for writing once every buffer period
    static auto last_check = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - last_check).count();

    if (duration < 1000000 * m_frames_per_buffer / m_sample_rate) {
        return false;
    }

    last_check = now;

    // Check if the CSV file is ready for writing
    if (!m_csv_file.is_open()) {
        return false;
    } else if (!m_is_running) {
        return false;
    } else {
        return true;
    }
}

void CSVAudioOutput::push(const float* data) {
    if (!m_csv_file.is_open()) {
        std::cerr << "Error: CSV file not open." << std::endl;
        return;
    }

    if (!m_is_running) {
        return;
    }
    
    // Store ALL samples from the buffer in interleaved format
    for (unsigned int frame = 0; frame < m_frames_per_buffer*m_channels; ++frame) {
        m_audio_buffer.push_back(data[frame]);
    }
}
