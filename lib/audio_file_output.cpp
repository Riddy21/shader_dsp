#include <fstream>
#include <thread>

#include "audio_wav.h"
#include "audio_file_output.h"

AudioFileOutput::~AudioFileOutput() {
}

bool AudioFileOutput::open() {
    // Open file from m_filename
    std::ofstream m_file(m_filename, std::ios::binary);
}

bool AudioFileOutput::close() {
    // Close file
    m_file.close();
}

bool AudioFileOutput::start() {
    // Start writing audio data to the file
    if (!m_file.is_open()) {
        fprintf(stderr, "Error: File not open.\n");
        return false;
    }

    // Write the audio file header
    WAVHeader header;
    header.riff[0] = 'R';
    header.riff[1] = 'I';
    header.riff[2] = 'F';
    header.riff[3] = 'F';
    header.overall_size = 0;
    header.wave[0] = 'W';
    header.wave[1] = 'A';
    header.wave[2] = 'V';
    header.wave[3] = 'E';
    header.length_of_fmt = 16;
    header.format_type = 1;
    header.channels = m_channels;
    header.sample_rate = m_sample_rate;
    header.byte_rate = header.sample_rate * header.channels * header.bits_per_sample / 8;
    header.block_align = header.channels * header.bits_per_sample / 8;
    header.bits_per_sample = 32;
    header.data_size = 0;

    m_file.write((char *) &header, sizeof(WAVHeader));

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
}

bool AudioFileOutput::stop() {
    if (!m_file.is_open()) {
        fprintf(stderr, "Error: File not open.\n");
        return false;
    }
    // TODO: Stop writing audio data to the file
}