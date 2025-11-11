#include "audio_core/audio_tape.h"
#include "audio_output/audio_wav.h"
#include <fstream>
#include <cstring>
#include <algorithm>

AudioTape::AudioTape(const unsigned int frames_per_buffer,
                     const unsigned int sample_rate,
                     const unsigned int num_channels,
                     const std::optional<unsigned int> tape_size) :
    m_frames_per_buffer(frames_per_buffer),
    m_sample_rate(sample_rate),
    m_num_channels(num_channels) {
    m_data.resize(m_num_channels);
    if (tape_size.has_value()) {
        // Pre-size per-channel contiguous buffers
        for (auto &ch : m_data) ch.assign(tape_size.value(), 0.0f);
        m_fixed_size = true;
    } else {
        // Start empty per-channel; will grow dynamically
        for (auto &ch : m_data) ch.clear();
        m_fixed_size = false;
    }
}

std::shared_ptr<AudioTape> AudioTape::load_from_wav_file(const std::string& audio_filepath,
                                                          const unsigned int frames_per_buffer,
                                                          const unsigned int sample_rate,
                                                          const std::optional<float> start_seconds,
                                                          const std::optional<float> end_seconds) {
    // Open the audio file
    std::ifstream file(audio_filepath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open audio file: " << audio_filepath << std::endl;
        return nullptr;
    }

    // Read the audio file header
    WAVHeader header;
    file.read((char *) &header, sizeof(WAVHeader));

    if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0) {
        std::cerr << "Invalid audio file format: " << audio_filepath << std::endl;
        return nullptr;
    }

    if (header.format_type != 1) {
        std::cerr << "Invalid audio file format type (only PCM supported): " << audio_filepath << std::endl;
        return nullptr;
    }

    // Print info
    std::cout << "Loading audio file: " << audio_filepath << std::endl;
    std::cout << "Channels: " << header.channels << std::endl;
    std::cout << "Sample rate: " << header.sample_rate << std::endl;
    std::cout << "Bits per sample: " << header.bits_per_sample << std::endl;
    std::cout << "Data size: " << header.data_size << std::endl;

    // Validate sample rate matches
    if (header.sample_rate != sample_rate) {
        std::cerr << "Warning: WAV file sample rate (" << header.sample_rate 
                  << ") does not match requested sample rate (" << sample_rate << ")" << std::endl;
    }

    // Validate bits per sample (only 16-bit supported)
    if (header.bits_per_sample != 16) {
        std::cerr << "Unsupported bits per sample: " << header.bits_per_sample 
                  << " (only 16-bit PCM supported)" << std::endl;
        return nullptr;
    }

    const unsigned int num_channels = header.channels;
    if (num_channels == 0) {
        std::cerr << "Invalid number of channels: 0" << std::endl;
        return nullptr;
    }

    // Calculate total duration and sample range
    const unsigned int total_samples = header.data_size / sizeof(int16_t);
    const unsigned int total_samples_per_channel = total_samples / num_channels;
    const float total_duration_seconds = static_cast<float>(total_samples_per_channel) / static_cast<float>(header.sample_rate);
    
    // Calculate start and end sample indices
    const float start_time = start_seconds.value_or(0.0f);
    const float end_time = end_seconds.value_or(total_duration_seconds);
    
    // Validate start and end times
    if (start_time < 0.0f) {
        std::cerr << "Invalid start time: " << start_time << " (must be >= 0)" << std::endl;
        return nullptr;
    }
    if (end_time <= start_time) {
        std::cerr << "Invalid end time: " << end_time << " (must be > start time: " << start_time << ")" << std::endl;
        return nullptr;
    }
    if (end_time > total_duration_seconds) {
        std::cerr << "Warning: End time (" << end_time 
                  << ") exceeds file duration (" << total_duration_seconds 
                  << "). Clamping to file duration." << std::endl;
    }
    
    // Calculate sample indices (in samples per channel)
    const unsigned int start_sample = static_cast<unsigned int>(start_time * static_cast<float>(header.sample_rate));
    const unsigned int end_sample = static_cast<unsigned int>(std::min(end_time, total_duration_seconds) * static_cast<float>(header.sample_rate));
    const unsigned int samples_to_load = end_sample - start_sample;
    
    if (start_sample >= total_samples_per_channel) {
        std::cerr << "Invalid start sample: " << start_sample 
                  << " (file has " << total_samples_per_channel << " samples per channel)" << std::endl;
        return nullptr;
    }
    
    // Print info about the range being loaded
    std::cout << "Loading audio file: " << audio_filepath << std::endl;
    std::cout << "Channels: " << num_channels << std::endl;
    std::cout << "Sample rate: " << header.sample_rate << std::endl;
    std::cout << "Bits per sample: " << header.bits_per_sample << std::endl;
    std::cout << "Total file duration: " << total_duration_seconds << " seconds" << std::endl;
    std::cout << "Loading range: " << start_time << " to " << end_time << " seconds" << std::endl;
    std::cout << "Loading samples: " << start_sample << " to " << end_sample << " (" << samples_to_load << " samples per channel)" << std::endl;

    // Read the entire audio data first
    const unsigned int expected_samples = header.data_size / sizeof(int16_t);
    std::vector<int16_t> data(expected_samples);
    file.read((char *) data.data(), header.data_size);

    if (!file) {
        std::cerr << "Failed to read audio data from file: " << audio_filepath << std::endl;
        return nullptr;
    }

    // Verify we read the expected amount of data
    const std::streamsize bytes_read = file.gcount();
    if (bytes_read != static_cast<std::streamsize>(header.data_size)) {
        std::cerr << "Warning: Expected " << header.data_size << " bytes, but read " 
                  << bytes_read << " bytes" << std::endl;
        // Adjust data size to what was actually read
        data.resize(bytes_read / sizeof(int16_t));
    }

    // Extract only the portion we want (between start_sample and end_sample)
    // Data is interleaved: [ch0_sample0, ch1_sample0, ch0_sample1, ch1_sample1, ...]
    std::vector<std::vector<float>> audio_data(num_channels, std::vector<float>(samples_to_load));
    
    for (unsigned int sample_idx = 0; sample_idx < samples_to_load; ++sample_idx) {
        const unsigned int global_sample_idx = start_sample + sample_idx;
        if (global_sample_idx >= total_samples_per_channel) {
            break; // Safety check
        }
        
        for (unsigned int ch = 0; ch < num_channels; ++ch) {
            // Calculate index in interleaved data array
            const unsigned int data_index = global_sample_idx * num_channels + ch;
            if (data_index < data.size()) {
                audio_data[ch][sample_idx] = data[data_index] / 32768.0f; // Convert from int16_t to float [-1.0, 1.0]
            } else {
                audio_data[ch][sample_idx] = 0.0f; // Pad with zeros if out of bounds
            }
        }
    }

    // Create AudioTape with the loaded data (size based on loaded portion)
    auto tape = std::make_shared<AudioTape>(frames_per_buffer, sample_rate, num_channels, samples_to_load);
    
    // Copy the loaded data into the tape
    for (unsigned int ch = 0; ch < num_channels; ++ch) {
        tape->m_data[ch] = std::move(audio_data[ch]);
    }
    
    // Set the record position to the end of the loaded data
    tape->m_current_record_position = samples_to_load;
    
    std::cout << "Successfully loaded " << samples_to_load << " samples per channel (" 
              << (static_cast<float>(samples_to_load) / static_cast<float>(header.sample_rate)) 
              << " seconds)" << std::endl;
    
    return tape;
}

void AudioTape::record(const float * audio_stream_data) {
    // Call the samples_offset version with current position
    record(audio_stream_data, m_current_record_position);
}

void AudioTape::record(const float * audio_stream_data, unsigned int samples_offset) {
    // Do differently depending on if the tape is fixed size or not
    if (m_fixed_size) {
        // Fixed window capacity per channel
        const unsigned int capacity = static_cast<unsigned int>(m_data.empty() ? 0 : m_data[0].size());
        if (capacity == 0) {
            return; // Nothing to do without capacity
        }

        // Current window in global sample index space
        unsigned int window_start = (m_current_record_position > capacity)
                                        ? (m_current_record_position - capacity)
                                        : 0u;
        unsigned int window_end_exclusive = window_start + capacity; // not inclusive

        const unsigned int write_start_global = samples_offset;
        const unsigned int write_end_global = write_start_global + m_frames_per_buffer; // exclusive

        // Shift window forward if needed (drop oldest)
        if (write_end_global > window_end_exclusive) {
            unsigned int shift = write_end_global - window_end_exclusive;
            if (shift >= capacity) {
                for (auto &ch : m_data) {
                    std::fill(ch.begin(), ch.end(), 0.0f);
                }
            } else {
                for (auto &ch : m_data) {
                    std::move(ch.begin() + shift, ch.end(), ch.begin());
                    std::fill(ch.end() - shift, ch.end(), 0.0f);
                }
            }
            window_start += shift;
            window_end_exclusive += shift;
        }

        // Shift window backward if needed (prepend zeros)
        if (write_start_global < window_start) {
            unsigned int shift_back = window_start - write_start_global;
            if (shift_back >= capacity) {
                for (auto &ch : m_data) {
                    std::fill(ch.begin(), ch.end(), 0.0f);
                }
            } else {
                for (auto &ch : m_data) {
                    std::move_backward(ch.begin(), ch.end() - shift_back, ch.end());
                    std::fill(ch.begin(), ch.begin() + shift_back, 0.0f);
                }
            }
            window_start -= shift_back;
        }

        // Local write start within window
        unsigned int local_start = write_start_global - window_start;

        // Write frames per channel via bulk copies (channel-major source)
        for (unsigned int ch = 0; ch < m_num_channels; ++ch) {
            const float* src = audio_stream_data + ch * m_frames_per_buffer;
            float* dst = m_data[ch].data() + local_start;
            std::copy_n(src, m_frames_per_buffer, dst);
        }

        if (m_current_record_position < write_end_global) {
            m_current_record_position = write_end_global;
        }
    } else {
        // Dynamic-size tape: ensure capacity per channel and copy
        const unsigned int write_start = samples_offset;
        const unsigned int write_end = write_start + m_frames_per_buffer; // exclusive

        for (auto &ch : m_data) {
            if (ch.size() < write_end) ch.resize(write_end, 0.0f);
        }

        for (unsigned int ch = 0; ch < m_num_channels; ++ch) {
            const float* src = audio_stream_data + ch * m_frames_per_buffer;
            float* dst = m_data[ch].data() + write_start;
            std::copy_n(src, m_frames_per_buffer, dst);
        }

        if (m_current_record_position < write_end) {
            m_current_record_position = write_end;
        }
    }
}

void AudioTape::record(const float * audio_stream_data, float seconds_offset) {
    // Convert seconds offset to samples offset
    unsigned int samples_offset = static_cast<unsigned int>(seconds_offset * m_sample_rate);
    this->record(audio_stream_data, samples_offset);
}

const std::vector<float> AudioTape::playback(const bool interleaved) const {
    return this->playback(m_frames_per_buffer, m_current_playback_position, interleaved);
}

const std::vector<float> AudioTape::playback(unsigned int samples_offset, const bool interleaved) const {
    return this->playback(m_frames_per_buffer, samples_offset, interleaved);
}

const std::vector<float> AudioTape::playback(float seconds_offset, const bool interleaved) const {
    unsigned int samples_offset = static_cast<unsigned int>(seconds_offset * m_sample_rate);
    return this->playback(m_frames_per_buffer, samples_offset, interleaved);
}

const std::vector<float> AudioTape::playback(unsigned int num_frames, unsigned int samples_offset, const bool interleaved) const {
    // Prepare output buffer in channel-major order: [ch0 frames][ch1 frames]...
    std::vector<float> output;
    const unsigned int frames_to_read = num_frames;
    if (frames_to_read == 0 || m_num_channels == 0) {
        return output;
    }
    output.assign(static_cast<std::size_t>(frames_to_read) * m_num_channels, 0.0f);

    const unsigned int start_global = samples_offset;

    // For fixed-size tape, compute the current visible window [window_start, window_end)
    const bool is_fixed = m_fixed_size;
    unsigned int window_start = 0;
    unsigned int window_end_exclusive = 0;
    unsigned int capacity = 0;
    if (is_fixed) {
        capacity = static_cast<unsigned int>(m_data.empty() ? 0 : m_data[0].size());
        if (capacity == 0) {
            return output; // nothing available
        }
        window_start = (m_current_record_position > capacity)
                           ? (m_current_record_position - capacity)
                           : 0u;
        window_end_exclusive = window_start + capacity;
    }

    for (unsigned int ch = 0; ch < m_num_channels; ++ch) {
        const auto &channel_data = m_data[ch];
        const unsigned int channel_size = static_cast<unsigned int>(channel_data.size());

        for (unsigned int i = 0; i < frames_to_read; ++i) {
            const unsigned int global_index = start_global + i;

            float sample_value = 0.0f;
            if (!is_fixed) {
                // Dynamic-size: direct index if within bounds
                if (global_index < channel_size) {
                    sample_value = channel_data[global_index];
                }
            } else {
                // Fixed-size sliding window
                if (global_index >= window_start && global_index < window_end_exclusive) {
                    const unsigned int local_index = global_index - window_start;
                    sample_value = channel_data[local_index];
                }
            }

            if (interleaved) {
                output[static_cast<std::size_t>(i) * m_num_channels + ch] = sample_value;
            } else {
                output[static_cast<std::size_t>(ch) * frames_to_read + i] = sample_value;
            }
        }
    }

    return output;
}

const std::vector<float> AudioTape::playback(unsigned int num_frames, float seconds_offset, const bool interleaved) const {
    unsigned int samples_offset = static_cast<unsigned int>(seconds_offset * m_sample_rate);
    return this->playback(num_frames, samples_offset, interleaved);
}

const std::vector<float> AudioTape::playback_for_render_stage_history(
    unsigned int window_size_samples,
    unsigned int samples_offset,
    unsigned int texture_width,
    unsigned int texture_rows_per_channel) const {
    
    // Calculate output size: texture_width * texture_height
    // texture_height = num_channels * texture_rows_per_channel * 2 (data rows + zero rows)
    const unsigned int texture_height = m_num_channels * texture_rows_per_channel * 2;
    const unsigned int total_output_size = texture_width * texture_height;
    
    std::vector<float> output(total_output_size, 0.0f);
    
    if (window_size_samples == 0 || m_num_channels == 0 || texture_width == 0 || texture_rows_per_channel == 0) {
        return output;
    }
    
    const unsigned int start_global = samples_offset;
    
    // For fixed-size tape, compute the current visible window [window_start, window_end)
    const bool is_fixed = m_fixed_size;
    unsigned int window_start = 0;
    unsigned int window_end_exclusive = 0;
    unsigned int capacity = 0;
    if (is_fixed) {
        capacity = static_cast<unsigned int>(m_data.empty() ? 0 : m_data[0].size());
        if (capacity == 0) {
            return output; // nothing available
        }
        window_start = (m_current_record_position > capacity)
                           ? (m_current_record_position - capacity)
                           : 0u;
        window_end_exclusive = window_start + capacity;
    }
    
    // Process row-by-row, interleaving channels
    // Format: Row 0: ch0, Row 1: zeros, Row 2: ch1, Row 3: zeros, Row 4: ch0, Row 5: zeros, Row 6: ch1, etc.
    for (unsigned int row = 0; row < texture_rows_per_channel; ++row) {
        // For each channel in this row
        for (unsigned int ch = 0; ch < m_num_channels; ++ch) {
            const auto &channel_data = m_data[ch];
            const unsigned int channel_size = static_cast<unsigned int>(channel_data.size());
            
            // Calculate texture row index: row * (num_channels * 2) + (channel * 2)
            // This interleaves channels: ch0 at row*4+0, zeros at row*4+1, ch1 at row*4+2, zeros at row*4+3
            const unsigned int texture_row = row * (m_num_channels * 2) + (ch * 2);
            
            // Calculate source sample range for this row
            const unsigned int source_start = row * texture_width;
            const unsigned int source_end = std::min(source_start + texture_width, window_size_samples);
            
            // Get pointer to destination row in texture
            float* dest_row = output.data() + (texture_row * texture_width);
            
            // Copy samples to texture row
            for (unsigned int i = 0; i < source_end - source_start; ++i) {
                const unsigned int global_index = start_global + source_start + i;
                
                float sample_value = 0.0f;
                if (!is_fixed) {
                    // Dynamic-size: direct index if within bounds
                    if (global_index < channel_size) {
                        sample_value = channel_data[global_index];
                    }
                } else {
                    // Fixed-size sliding window
                    if (global_index >= window_start && global_index < window_end_exclusive) {
                        const unsigned int local_index = global_index - window_start;
                        sample_value = channel_data[local_index];
                    }
                }
                
                dest_row[i] = sample_value;
            }
            
            // If we have fewer samples than texture width, repeat the last sample
            if (source_end < source_start + texture_width) {
                const unsigned int samples_copied = source_end - source_start;
                float last_sample = 0.0f;
                if (samples_copied > 0) {
                    last_sample = dest_row[samples_copied - 1];
                }
                std::fill(dest_row + samples_copied, dest_row + texture_width, last_sample);
            }
            // Zero row is already initialized to 0.0f by vector constructor above
        }
    }
    
    return output;
}

void AudioTape::clear() {
    for (auto &ch : m_data) {
        ch.clear();
    }
    m_current_record_position = 0;
    m_current_playback_position = 0;
}

bool AudioTape::export_to_wav_file(const std::string& output_filepath) const {
    // Check if tape has any data
    if (m_data.empty() || m_data[0].empty()) {
        std::cerr << "Cannot export empty tape to WAV file" << std::endl;
        return false;
    }
    
    // Use the WAV utility function to write the file
    return write_wav_file(output_filepath, m_data, m_sample_rate, m_num_channels);
}

