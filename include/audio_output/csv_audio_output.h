#pragma once

#include "audio_output/audio_output.h"
#include <fstream>
#include <vector>
#include <string>

class CSVAudioOutput : public AudioOutput {
public:
    CSVAudioOutput(unsigned int frames_per_buffer, unsigned int sample_rate, unsigned int channels, const std::string& filename = "audio_output.csv");
    ~CSVAudioOutput();
    
    bool open() override;
    bool start() override;
    bool stop() override;
    bool close() override;
    bool is_ready() override;
    void push(const float* data) override;
    
private:
    std::string m_filename;
    std::ofstream m_csv_file;
    std::vector<float> m_audio_buffer;
    bool m_is_running;
};
