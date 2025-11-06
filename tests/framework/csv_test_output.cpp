#include "csv_test_output.h"
#include <iostream>
#include <cmath>

CSVTestOutput::CSVTestOutput(const std::string& filename, unsigned int sample_rate)
    : m_filename(filename), m_sample_rate(sample_rate) {
    m_csv_file.open(filename);
    if (!m_csv_file.is_open()) {
        std::cerr << "Warning: Failed to open CSV file '" << filename << "' for writing" << std::endl;
    } else {
        std::cout << "Opened CSV file: " << filename << std::endl;
    }
}

CSVTestOutput::~CSVTestOutput() {
    close();
}

unsigned int CSVTestOutput::get_sample_rate(unsigned int override_rate) const {
    return override_rate > 0 ? override_rate : m_sample_rate;
}

void CSVTestOutput::write_header(const std::vector<std::string>& column_names) {
    if (!m_csv_file.is_open()) {
        std::cerr << "Error: CSV file not open" << std::endl;
        return;
    }
    
    for (size_t i = 0; i < column_names.size(); ++i) {
        m_csv_file << column_names[i];
        if (i < column_names.size() - 1) {
            m_csv_file << ",";
        }
    }
    m_csv_file << std::endl;
}

void CSVTestOutput::write_channels(const std::vector<std::vector<float>>& samples_per_channel, unsigned int sample_rate) {
    if (!m_csv_file.is_open()) {
        std::cerr << "Error: CSV file not open" << std::endl;
        return;
    }
    
    if (samples_per_channel.empty() || samples_per_channel[0].empty()) {
        std::cerr << "Warning: No samples to write" << std::endl;
        return;
    }
    
    unsigned int sr = get_sample_rate(sample_rate);
    unsigned int num_channels = samples_per_channel.size();
    unsigned int num_samples = samples_per_channel[0].size();
    
    // Write header - use format compatible with analyze_discontinuities.py
    // For stereo: frame,time_seconds,left_channel,right_channel
    // For mono: frame,time_seconds,amplitude
    // For other: frame,time_seconds,channel_0,channel_1,...
    std::vector<std::string> header = {"frame", "time_seconds"};
    if (num_channels == 1) {
        header.push_back("amplitude");
    } else if (num_channels == 2) {
        header.push_back("left_channel");
        header.push_back("right_channel");
    } else {
        for (unsigned int ch = 0; ch < num_channels; ++ch) {
            header.push_back("channel_" + std::to_string(ch));
        }
    }
    write_header(header);
    
    // Write data
    for (unsigned int i = 0; i < num_samples; ++i) {
        double time_seconds = static_cast<double>(i) / sr;
        
        m_csv_file << i << "," << std::fixed << std::setprecision(9) << time_seconds;
        
        for (unsigned int ch = 0; ch < num_channels; ++ch) {
            if (i < samples_per_channel[ch].size()) {
                m_csv_file << "," << samples_per_channel[ch][i];
            } else {
                m_csv_file << ",0.0";
            }
        }
        m_csv_file << std::endl;
    }
    
    std::cout << "Wrote " << num_samples << " samples (" << num_channels << " channels) to " << m_filename << std::endl;
}

void CSVTestOutput::write_interleaved(const std::vector<float>& interleaved_samples, unsigned int num_channels, unsigned int sample_rate) {
    if (!m_csv_file.is_open()) {
        std::cerr << "Error: CSV file not open" << std::endl;
        return;
    }
    
    if (interleaved_samples.empty() || num_channels == 0) {
        std::cerr << "Warning: No samples to write" << std::endl;
        return;
    }
    
    unsigned int sr = get_sample_rate(sample_rate);
    unsigned int num_samples = interleaved_samples.size() / num_channels;
    
    // Write header
    std::vector<std::string> header = {"sample_index", "time_seconds"};
    for (unsigned int ch = 0; ch < num_channels; ++ch) {
        header.push_back("channel_" + std::to_string(ch));
    }
    write_header(header);
    
    // Write data (de-interleave)
    for (unsigned int i = 0; i < num_samples; ++i) {
        double time_seconds = static_cast<double>(i) / sr;
        
        m_csv_file << i << "," << std::fixed << std::setprecision(9) << time_seconds;
        
        for (unsigned int ch = 0; ch < num_channels; ++ch) {
            unsigned int idx = i * num_channels + ch;
            if (idx < interleaved_samples.size()) {
                m_csv_file << "," << interleaved_samples[idx];
            } else {
                m_csv_file << ",0.0";
            }
        }
        m_csv_file << std::endl;
    }
    
    std::cout << "Wrote " << num_samples << " samples (" << num_channels << " channels) to " << m_filename << std::endl;
}

void CSVTestOutput::write_frames(const std::vector<std::vector<float>>& samples_per_channel, unsigned int frames_per_buffer, unsigned int sample_rate) {
    if (!m_csv_file.is_open()) {
        std::cerr << "Error: CSV file not open" << std::endl;
        return;
    }
    
    if (samples_per_channel.empty() || samples_per_channel[0].empty()) {
        std::cerr << "Warning: No samples to write" << std::endl;
        return;
    }
    
    unsigned int sr = get_sample_rate(sample_rate);
    unsigned int num_channels = samples_per_channel.size();
    unsigned int total_samples = samples_per_channel[0].size();
    
    // Write header - compatible with analyze_discontinuities.py
    std::vector<std::string> header;
    if (num_channels == 1) {
        header = {"frame", "time_seconds", "amplitude"};
    } else if (num_channels == 2) {
        header = {"frame", "time_seconds", "left_channel", "right_channel"};
    } else {
        header = {"frame", "time_seconds"};
        for (unsigned int ch = 0; ch < num_channels; ++ch) {
            header.push_back("channel_" + std::to_string(ch));
        }
    }
    write_header(header);
    
    // Write data frame by frame
    unsigned int frame_index = 0;
    for (unsigned int i = 0; i < total_samples; i += frames_per_buffer) {
        unsigned int frame_start = i;
        unsigned int frame_end = std::min(i + frames_per_buffer, total_samples);
        
        // Calculate time at the start of the frame
        double time_seconds = static_cast<double>(frame_start) / sr;
        
        m_csv_file << frame_index << "," << std::fixed << std::setprecision(9) << time_seconds;
        
        // For multi-channel, write each channel's first sample in this frame
        // This matches the frame-based analysis format
        if (frame_start < samples_per_channel[0].size()) {
            for (unsigned int ch = 0; ch < num_channels; ++ch) {
                if (frame_start < samples_per_channel[ch].size()) {
                    m_csv_file << "," << samples_per_channel[ch][frame_start];
                } else {
                    m_csv_file << ",0.0";
                }
            }
        }
        m_csv_file << std::endl;
        
        frame_index++;
    }
    
    std::cout << "Wrote " << frame_index << " frames (" << total_samples << " samples total) to " << m_filename << std::endl;
}

void CSVTestOutput::write_with_metadata(const std::vector<std::vector<float>>& samples_per_channel,
                                        const std::vector<std::string>& metadata_columns,
                                        const std::vector<std::vector<float>>& metadata_values,
                                        unsigned int sample_rate) {
    if (!m_csv_file.is_open()) {
        std::cerr << "Error: CSV file not open" << std::endl;
        return;
    }
    
    if (samples_per_channel.empty() || samples_per_channel[0].empty()) {
        std::cerr << "Warning: No samples to write" << std::endl;
        return;
    }
    
    unsigned int sr = get_sample_rate(sample_rate);
    unsigned int num_channels = samples_per_channel.size();
    unsigned int num_samples = samples_per_channel[0].size();
    
    // Write header
    std::vector<std::string> header = {"sample_index", "time_seconds"};
    for (unsigned int ch = 0; ch < num_channels; ++ch) {
        header.push_back("channel_" + std::to_string(ch));
    }
    for (const auto& meta_col : metadata_columns) {
        header.push_back(meta_col);
    }
    write_header(header);
    
    // Write data
    for (unsigned int i = 0; i < num_samples; ++i) {
        double time_seconds = static_cast<double>(i) / sr;
        
        m_csv_file << i << "," << std::fixed << std::setprecision(9) << time_seconds;
        
        // Write channel data
        for (unsigned int ch = 0; ch < num_channels; ++ch) {
            if (i < samples_per_channel[ch].size()) {
                m_csv_file << "," << samples_per_channel[ch][i];
            } else {
                m_csv_file << ",0.0";
            }
        }
        
        // Write metadata
        for (size_t meta_idx = 0; meta_idx < metadata_columns.size(); ++meta_idx) {
            if (meta_idx < metadata_values.size() && i < metadata_values[meta_idx].size()) {
                m_csv_file << "," << metadata_values[meta_idx][i];
            } else {
                m_csv_file << ",0.0";
            }
        }
        m_csv_file << std::endl;
    }
    
    std::cout << "Wrote " << num_samples << " samples with metadata to " << m_filename << std::endl;
}

void CSVTestOutput::write_input_output(const std::vector<float>& input_samples, const std::vector<float>& output_samples) {
    if (!m_csv_file.is_open()) {
        std::cerr << "Error: CSV file not open" << std::endl;
        return;
    }
    
    unsigned int num_samples = std::min(input_samples.size(), output_samples.size());
    
    if (num_samples == 0) {
        std::cerr << "Warning: No samples to write" << std::endl;
        return;
    }
    
    // Write header - compatible with read_audio_csv.py combined mode
    write_header({"sample_index", "input", "output"});
    
    // Write data
    for (unsigned int i = 0; i < num_samples; ++i) {
        m_csv_file << i << "," << input_samples[i] << "," << output_samples[i] << std::endl;
    }
    
    std::cout << "Wrote " << num_samples << " input/output sample pairs to " << m_filename << std::endl;
}

void CSVTestOutput::close() {
    if (m_csv_file.is_open()) {
        m_csv_file.close();
        std::cout << "Closed CSV file: " << m_filename << std::endl;
    }
}

