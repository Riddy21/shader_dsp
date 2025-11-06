#ifndef CSV_TEST_OUTPUT_H
#define CSV_TEST_OUTPUT_H

#include <string>
#include <vector>
#include <fstream>
#include <iomanip>
#include <sstream>

/**
 * @brief Utility class for writing test output data to CSV files
 * 
 * Provides methods to write audio samples, channel data, and other test output
 * to CSV files that can be analyzed by Python scripts in the playground directory.
 */
class CSVTestOutput {
public:
    /**
     * @brief Construct a CSV writer
     * @param filename Path to the CSV file to write
     * @param sample_rate Sample rate for time calculations (default: 44100)
     */
    CSVTestOutput(const std::string& filename, unsigned int sample_rate = 44100);
    
    ~CSVTestOutput();
    
    /**
     * @brief Write audio samples per channel to CSV
     * Format: sample_index,time_seconds,channel_0,channel_1,...
     * Compatible with read_audio_csv.py and analyze_discontinuities.py
     * 
     * @param samples_per_channel Vector of vectors, where each inner vector contains samples for one channel
     * @param sample_rate Optional sample rate (overrides constructor value)
     */
    void write_channels(const std::vector<std::vector<float>>& samples_per_channel, unsigned int sample_rate = 0);
    
    /**
     * @brief Write interleaved audio samples to CSV
     * Format: sample_index,time_seconds,channel_0,channel_1,...
     * 
     * @param interleaved_samples Interleaved samples: [sample0_ch0, sample0_ch1, sample1_ch0, sample1_ch1, ...]
     * @param num_channels Number of audio channels
     * @param sample_rate Optional sample rate (overrides constructor value)
     */
    void write_interleaved(const std::vector<float>& interleaved_samples, unsigned int num_channels, unsigned int sample_rate = 0);
    
    /**
     * @brief Write audio samples with frame-based format
     * Format: frame,time_seconds,left_channel,right_channel (for stereo)
     * Compatible with analyze_discontinuities.py
     * 
     * @param samples_per_channel Vector of vectors containing samples per channel
     * @param frames_per_buffer Number of frames per buffer
     * @param sample_rate Optional sample rate (overrides constructor value)
     */
    void write_frames(const std::vector<std::vector<float>>& samples_per_channel, unsigned int frames_per_buffer, unsigned int sample_rate = 0);
    
    /**
     * @brief Write audio with additional metadata columns
     * Format: sample_index,time_seconds,channel_0,...,metadata_columns...
     * 
     * @param samples_per_channel Vector of vectors containing samples per channel
     * @param metadata_columns Vector of metadata column names
     * @param metadata_values Vector of vectors, where each inner vector contains metadata values for each sample
     * @param sample_rate Optional sample rate (overrides constructor value)
     */
    void write_with_metadata(const std::vector<std::vector<float>>& samples_per_channel,
                            const std::vector<std::string>& metadata_columns,
                            const std::vector<std::vector<float>>& metadata_values,
                            unsigned int sample_rate = 0);
    
    /**
     * @brief Write simple format: sample_index,input,output
     * Compatible with read_audio_csv.py combined mode
     * 
     * @param input_samples Input signal samples
     * @param output_samples Output signal samples
     */
    void write_input_output(const std::vector<float>& input_samples, const std::vector<float>& output_samples);
    
    /**
     * @brief Check if the file is open and ready for writing
     */
    bool is_open() const { return m_csv_file.is_open(); }
    
    /**
     * @brief Close the CSV file explicitly (automatically called by destructor)
     */
    void close();

private:
    std::ofstream m_csv_file;
    std::string m_filename;
    unsigned int m_sample_rate;
    
    unsigned int get_sample_rate(unsigned int override_rate) const;
    void write_header(const std::vector<std::string>& column_names);
};

#endif // CSV_TEST_OUTPUT_H

