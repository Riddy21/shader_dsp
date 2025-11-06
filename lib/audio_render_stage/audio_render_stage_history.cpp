#include "audio_render_stage/audio_render_stage_history.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include <algorithm>
#include <optional>
#include <cmath>
#include <stdexcept>
#include <string>
#include <limits>

AudioRenderStageHistory::AudioRenderStageHistory(const unsigned int history_size,
                                                 const unsigned int frames_per_buffer,
                                                 const unsigned int sample_rate,
                                                 const unsigned int num_channels) :
    m_frames_per_buffer(frames_per_buffer),
    m_sample_rate(sample_rate),
    m_num_channels(num_channels),
    m_texture_rows(history_size/MAX_TEXTURE_SIZE) {

    // Check history_size must be multiple of MAX_TEXTURE_SIZE
    if (history_size % MAX_TEXTURE_SIZE != 0) {
        throw std::invalid_argument("History size must be a multiple of MAX_TEXTURE_SIZE");
    }

    m_history_buffer.resize(num_channels);
    for (int i = 0; i < num_channels; i++) {
        m_history_buffer[i].resize(history_size);
    }
}

AudioTexture2DParameter * AudioRenderStageHistory::create_audio_history_texture(GLuint m_active_texture_count) {
    auto audio_history_texture = new AudioTexture2DParameter("audio_history_texture",
                            AudioParameter::ConnectionType::INPUT,
                            MAX_TEXTURE_SIZE, m_num_channels * m_texture_rows, // Due to restriction of the shader only can be as big as the buffer size
                            m_active_texture_count,
                            0, GL_NEAREST);

    float* temp_buffer = new float[MAX_TEXTURE_SIZE * m_num_channels * m_texture_rows];
    audio_history_texture->set_value(temp_buffer);
    delete[] temp_buffer;

    m_audio_history_texture = audio_history_texture;

    return audio_history_texture;
}

void AudioRenderStageHistory::shift_history_buffer() {
    for (int i = 0; i < m_num_channels; i++) {
        std::copy(m_history_buffer[i].begin() + m_frames_per_buffer, m_history_buffer[i].end(), m_history_buffer[i].begin());
    }
}

void AudioRenderStageHistory::save_stream_to_history(const float * audio_stream_data) {
    for (int i = 0; i < m_num_channels; i++) {
        const float * channel_pointer = audio_stream_data + i * m_frames_per_buffer;
        std::copy(channel_pointer, channel_pointer + m_frames_per_buffer, m_history_buffer[i].end() - m_frames_per_buffer);
    }
}

const std::vector<float> AudioRenderStageHistory::get_history_data() {
    std::vector<float> total_data;
    total_data.reserve(MAX_TEXTURE_SIZE * m_num_channels * m_texture_rows);

    for (unsigned int row = 0; row < m_texture_rows; ++row) {
        for (unsigned int channel = 0; channel < m_num_channels; ++channel) {
            unsigned int start_index = row * MAX_TEXTURE_SIZE;
            unsigned int end_index = start_index + MAX_TEXTURE_SIZE;
            total_data.insert(total_data.end(), m_history_buffer[channel].begin() + start_index, m_history_buffer[channel].begin() + end_index);
        }
    }

    return total_data;
}

void AudioRenderStageHistory::update_audio_history_texture() {
    m_audio_history_texture->set_value(get_history_data().data());
}

void AudioRenderStageHistory::clear_history_buffer() {
    for (int i = 0; i < m_num_channels; i++) {
        std::fill(m_history_buffer[i].begin(), m_history_buffer[i].end(), 0.0f);
    }
}

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

// AudioRenderStageHistory2 implementation
AudioRenderStageHistory2::AudioRenderStageHistory2(const unsigned int frames_per_buffer,
                                                   const unsigned int sample_rate,
                                                   const unsigned int num_channels,
                                                   const float history_buffer_size_seconds)
    : m_frames_per_buffer(frames_per_buffer),
      m_sample_rate(sample_rate),
      m_num_channels(num_channels),
      m_audio_history_texture(nullptr),
      m_tape_position(nullptr),
      m_tape_window_size_samples(nullptr),
      m_tape_speed(nullptr),
      m_tape_window_offset_samples(nullptr) {
    
    // Convert seconds to samples
    float buffer_size_seconds = history_buffer_size_seconds;
    float min_buffer_seconds = static_cast<float>(frames_per_buffer) / static_cast<float>(sample_rate);
    if (buffer_size_seconds < min_buffer_seconds) {
        buffer_size_seconds = min_buffer_seconds;
    }
    unsigned int total_samples = static_cast<unsigned int>(buffer_size_seconds * static_cast<float>(sample_rate));

    m_texture_width = std::min<unsigned int>(MAX_TEXTURE_SIZE, static_cast<unsigned int>(total_samples));
    
    m_texture_rows_per_channel = total_samples / m_texture_width; // Will round down to the nearest width
    m_texture_height = m_num_channels * m_texture_rows_per_channel * 2; // 2 because we need to store both the audio data and the zeros

    m_window_size_samples = m_texture_width * m_texture_rows_per_channel;
}

void AudioRenderStageHistory2::create_parameters(GLuint& active_texture_count) {
    // Create the texture parameter
    auto audio_history_texture = new AudioTexture2DParameter(m_audio_history_texture_name,
                            AudioParameter::ConnectionType::INPUT,
                            m_texture_width, m_texture_height,
                            active_texture_count,
                            0, GL_LINEAR);
    
    // Initialize with zeros
    const unsigned int total_size = m_texture_width * m_texture_height;
    float* temp_buffer = new float[total_size];
    std::fill(temp_buffer, temp_buffer + total_size, 0.0f);
    audio_history_texture->set_value(temp_buffer);
    delete[] temp_buffer;
    
    m_audio_history_texture = audio_history_texture;
    
    // Create uniform parameters for tape control
    m_tape_position = new AudioIntParameter("tape_position", AudioParameter::ConnectionType::INPUT);
    static_cast<AudioIntParameter*>(m_tape_position)->set_value(0);
    
    // Store samples per buffer (default = frames_per_buffer, which means speed = 1.0)
    m_tape_speed = new AudioIntParameter("speed_in_samples_per_buffer", AudioParameter::ConnectionType::INPUT);
    static_cast<AudioIntParameter*>(m_tape_speed)->set_value(static_cast<int>(m_frames_per_buffer));
    
    // Calculate window size in samples based on texture dimensions
    m_tape_window_size_samples = new AudioIntParameter("tape_window_size_samples", AudioParameter::ConnectionType::INPUT);
    static_cast<AudioIntParameter*>(m_tape_window_size_samples)->set_value(static_cast<int>(m_window_size_samples));
    
    m_tape_window_offset_samples = new AudioIntParameter("tape_window_offset_samples", AudioParameter::ConnectionType::INPUT);
    // Set to large value to ensure texture updates on first frame
    static_cast<AudioIntParameter*>(m_tape_window_offset_samples)->set_value(1000000000);
    
    // Increment active_texture_count for the texture we just created
    active_texture_count++;
}

std::vector<AudioParameter*> AudioRenderStageHistory2::get_parameters() const {
    std::vector<AudioParameter*> params;
    if (m_audio_history_texture) params.push_back(m_audio_history_texture);
    if (m_tape_position) params.push_back(m_tape_position);
    if (m_tape_speed) params.push_back(m_tape_speed);
    if (m_tape_window_size_samples) params.push_back(m_tape_window_size_samples);
    if (m_tape_window_offset_samples) params.push_back(m_tape_window_offset_samples);
    return params;
}

void AudioRenderStageHistory2::set_tape_position(const unsigned int tape_position) {
    if (m_tape_position) {
        static_cast<AudioIntParameter*>(m_tape_position)->set_value(static_cast<int>(tape_position));
    }
}

void AudioRenderStageHistory2::set_tape_position(const float seconds_offset) {
    unsigned int samples_offset = static_cast<unsigned int>(seconds_offset * m_sample_rate);
    set_tape_position(samples_offset);
}

const unsigned int AudioRenderStageHistory2::get_tape_position() const {
    if (m_tape_position) {
        return static_cast<unsigned int>(*static_cast<const int*>(m_tape_position->get_value()));
    }
    return 0;
}

const float AudioRenderStageHistory2::get_tape_position_in_seconds() const {
    return static_cast<float>(get_tape_position()) / static_cast<float>(m_sample_rate);
}

void AudioRenderStageHistory2::set_tape_speed(const float speed) {
    if (m_tape_speed) {
        // Convert speed to samples per buffer: samples_per_buffer = speed * frames_per_buffer
        int samples_per_buffer = static_cast<int>(speed * static_cast<float>(m_frames_per_buffer));
        static_cast<AudioIntParameter*>(m_tape_speed)->set_value(samples_per_buffer);
    }
}

const float AudioRenderStageHistory2::get_tape_speed_ratio() const {
    if (m_tape_speed) {
        // Convert samples per buffer to speed ratio: speed = samples_per_buffer / frames_per_buffer
        int samples_per_buffer = *static_cast<const int*>(m_tape_speed->get_value());
        return static_cast<float>(samples_per_buffer) / static_cast<float>(m_frames_per_buffer);
    }
    return 1.0f;
}

const float AudioRenderStageHistory2::get_tape_speed_samples_per_second() const {
    if (m_tape_speed) {
        // Convert samples per buffer to samples per second: samples_per_second = samples_per_buffer * sample_rate / frames_per_buffer
        int samples_per_buffer = *static_cast<const int*>(m_tape_speed->get_value());
        return static_cast<float>(samples_per_buffer) * static_cast<float>(m_sample_rate) / static_cast<float>(m_frames_per_buffer);
    }
    return static_cast<float>(m_sample_rate);
}

const int AudioRenderStageHistory2::get_tape_speed_samples_per_buffer() const {
    if (m_tape_speed) {
        int samples_per_buffer = *static_cast<const int*>(m_tape_speed->get_value());
        return samples_per_buffer;
    }
    return static_cast<int>(m_frames_per_buffer);
}

const unsigned int AudioRenderStageHistory2::get_window_size_samples() const {
    if (m_tape_window_size_samples) {
        return static_cast<unsigned int>(*static_cast<const int*>(m_tape_window_size_samples->get_value()));
    }
    return 0;
}

const float AudioRenderStageHistory2::get_window_size_seconds() const {
    unsigned int window_size_samples = get_window_size_samples();
    return static_cast<float>(window_size_samples) / static_cast<float>(m_sample_rate);
}

const unsigned int AudioRenderStageHistory2::get_window_offset_samples() const {
    if (m_tape_window_offset_samples) {
        return static_cast<unsigned int>(*static_cast<const int*>(m_tape_window_offset_samples->get_value()));
    }
    return 0;
}

const float AudioRenderStageHistory2::get_window_offset_seconds() const {
    unsigned int window_offset_samples = get_window_offset_samples();
    return static_cast<float>(window_offset_samples) / static_cast<float>(m_sample_rate);
}

void AudioRenderStageHistory2::set_window_offset_samples(const unsigned int window_offset_samples) {
    if (m_tape_window_offset_samples) {
        static_cast<AudioIntParameter*>(m_tape_window_offset_samples)->set_value(static_cast<int>(window_offset_samples));
    }
}

// TODO: Change this to use mtime
void AudioRenderStageHistory2::update_audio_history_texture(const unsigned int time) {
    // Steps needed:
    // 1. Check if texture is created
    // 2. Check if tape is assigned (lock weak_ptr)
    // 3. Calculate read window based on current position and window size
    // 4. Read audio data from tape using playback()
    // 5. Convert from channel-major order to texture format (matching AudioRenderStageHistory format)
    // 6. Update texture with converted data
    // 7. Advance tape position based on playback speed (if speed != 0)
    // 8. Update uniform parameters with new position

    // If the speed is 0, then don't update the texture
    if (get_tape_speed_ratio() == 0.0f) {
        return;
    }
    
    if (!m_audio_history_texture) {
        return; // Texture not created yet
    }

    auto tape = m_tape.lock();
    if (!tape) {
        return; // Tape not assigned
    }
    
    // FIXME: If we swap the tape need to update the tape position and audio data in the texture
    // TODO: Add wraparound if wanted

    // Get current position and speed before any updates
    int speed_samples = get_tape_speed_samples_per_buffer();
    unsigned int current_position = get_tape_position();

    // Check boundaries BEFORE updating texture to avoid updating at boundaries
    // Handle negative speed: clamp to 0 minimum
    if (speed_samples < 0 && static_cast<unsigned int>(-speed_samples) > current_position) {
        set_tape_position(0u);
        return;
    } else if (speed_samples > 0 && current_position + speed_samples >= tape->size()) {
        set_tape_position(tape->size());
        return;
    }

    // Advance tape position based on tape speed (can be negative for reverse playback)
    set_tape_position(current_position + speed_samples);

    // Check if the audio history texture needs to be updated
    if (is_audio_texture_data_outdated()) {
        // FIXME: Positive offset samples are not working correctly for +ve speeds
        auto window_offset_samples = get_window_offset_samples_for_tape_data();

        // Get the piece of tape data directly in texture format
        std::vector<float> texture_data = tape->playback_for_render_stage_history(
            get_window_size_samples(),
            window_offset_samples,
            m_texture_width,
            m_texture_rows_per_channel);

        // Update the texture directly with the formatted data
        if (m_audio_history_texture) {
            static_cast<AudioTexture2DParameter*>(m_audio_history_texture)->set_value(texture_data.data());
        }

        // Update the window offset samples to the current tape position
        set_window_offset_samples(window_offset_samples);
    }
}

bool AudioRenderStageHistory2::is_audio_texture_data_outdated() {
    unsigned int tape_position = get_tape_position();

    // Get samples per buffer directly from parameter (can be negative)
    int samples_per_buffer = get_tape_speed_samples_per_buffer();
    unsigned int frame_size_samples = static_cast<unsigned int>(std::abs(samples_per_buffer));

    unsigned int window_offset = get_window_offset_samples();
    unsigned int window_size = get_window_size_samples();
    
    // Calculate valid range: we need margin at both ends to ensure smooth playback
    // The valid range excludes frame_size_samples from each end to provide safety margin
    // This prevents accessing samples outside the window during rendering
    unsigned int valid_start = window_offset + frame_size_samples;
    unsigned int valid_end = window_offset + window_size - frame_size_samples;
    
    // Check if we're outside the valid range OR approaching the boundary
    // Update preemptively when within 2 buffer sizes of the boundary to ensure continuity
    // This prevents discontinuities when the window updates mid-render
    unsigned int safety_margin = frame_size_samples * 2;
    
    // For forward playback: check if we're approaching the end or have passed start
    if (samples_per_buffer >= 0) {
        bool past_start = tape_position < valid_start;
        bool near_end = tape_position >= (valid_end > safety_margin ? valid_end - safety_margin : 0);
        return past_start || near_end;
    } else {
        // For reverse playback: check if we're approaching the start or have passed end
        bool past_end = tape_position >= valid_end;
        bool near_start = tape_position < (valid_start + safety_margin);
        return past_end || near_start;
    }
}

const unsigned int AudioRenderStageHistory2::get_window_offset_samples_for_tape_data() const {
    unsigned int tape_position = get_tape_position();

    float speed_ratio = get_tape_speed_ratio();
    if (speed_ratio > 0.0f) {
        // For positive speed, offset is position - 1, but clamp to 0 minimum to avoid underflow
        if (tape_position == 0) {
            return 0;
        }
        return tape_position - 1;
    } else if (speed_ratio < 0.0f) {
        unsigned int window_size = get_window_size_samples();
        // For negative speed, offset is position - window_size, but clamp to 0 minimum
        if (tape_position < window_size) {
            return 0;
        }
        return tape_position - window_size;
    }
    return 0;

}
