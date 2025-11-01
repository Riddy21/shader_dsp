#include "audio_render_stage/audio_render_stage_history.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include <algorithm>
#include <optional>
#include <cmath>
#include <stdexcept>
#include <string>

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

void AudioTape::record(const float * audio_stream_data, std::optional<unsigned int> samples_offset) {
    // Append to end of the tape if no offset is provided
    if (!samples_offset.has_value()) {
        samples_offset = m_current_record_position;
    }

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

        const unsigned int write_start_global = samples_offset.value();
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
        const unsigned int write_start = samples_offset.value();
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

void AudioTape::record(const float * audio_stream_data, std::optional<float> seconds_offset) {
    // Convert seconds offset to samples offset
    if (seconds_offset.has_value()) {
        unsigned int samples_offset = static_cast<unsigned int>(seconds_offset.value() * m_sample_rate);
        this->record(audio_stream_data, std::optional<unsigned int>{samples_offset});
    } else {
        this->record(audio_stream_data, std::optional<unsigned int>{});
    }
}

const std::vector<float> AudioTape::playback(std::optional<unsigned int> num_frames, std::optional<unsigned int> samples_offset, const bool interleaved) const {
    // Set the sample offset to the current playback position if not set
    if (!samples_offset.has_value()) {
        samples_offset = m_current_playback_position;
    }

    // Prepare output buffer in channel-major order: [ch0 frames][ch1 frames]...
    std::vector<float> output;
    const unsigned int frames_to_read = num_frames.has_value() ? num_frames.value() : m_frames_per_buffer;
    if (frames_to_read == 0 || m_num_channels == 0) {
        return output;
    }
    output.assign(static_cast<std::size_t>(frames_to_read) * m_num_channels, 0.0f);

    const unsigned int start_global = samples_offset.value();

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

const std::vector<float> AudioTape::playback(std::optional<unsigned int> num_frames, std::optional<float> seconds_offset, const bool interleaved) const {
    // Convert seconds offset to samples offset
    if (seconds_offset.has_value()) {
        unsigned int samples_offset = static_cast<unsigned int>(seconds_offset.value() * m_sample_rate);
        return this->playback(num_frames, std::optional<unsigned int>{samples_offset}, interleaved);
    } else {
        return this->playback(num_frames, std::optional<unsigned int>{}, interleaved);
    }
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
      m_tape_window_size_seconds(nullptr),
      m_tape_speed(nullptr),
      m_current_tape_position_samples(0),
      m_current_tape_speed(1.0f) {
    
    // Convert seconds to samples
    float buffer_size_seconds = history_buffer_size_seconds;
    float min_buffer_seconds = static_cast<float>(frames_per_buffer) / static_cast<float>(sample_rate);
    if (buffer_size_seconds < min_buffer_seconds) {
        buffer_size_seconds = min_buffer_seconds;
    }
    unsigned int total_samples = static_cast<unsigned int>(buffer_size_seconds * static_cast<float>(sample_rate));
    
    // Calculate number of rows needed per channel
    unsigned int rows_per_channel = (total_samples + MAX_TEXTURE_SIZE - 1) / MAX_TEXTURE_SIZE;
    
    // Calculate texture height (num_channels * rows_per_channel)
    unsigned int texture_height = m_num_channels * rows_per_channel;
    
    // Round up texture height to match width (make it square)
    // Round up to nearest multiple of num_channels that is >= texture_height and <= MAX_TEXTURE_SIZE
    if (texture_height > MAX_TEXTURE_SIZE) {
        throw std::invalid_argument(
            "History buffer size too large: requires texture height " + std::to_string(texture_height) + 
            " which exceeds MAX_TEXTURE_SIZE " + std::to_string(MAX_TEXTURE_SIZE)
        );
    }
    
    // Round up texture_height to make texture square (height = width = MAX_TEXTURE_SIZE)
    // But ensure it's still a multiple of num_channels
    unsigned int final_height = MAX_TEXTURE_SIZE;
    // Ensure final_height is a multiple of num_channels
    if (final_height % m_num_channels != 0) {
        final_height = final_height - (final_height % m_num_channels);
    }
    
    // Recalculate rows_per_channel based on final height
    unsigned int final_rows_per_channel = final_height / m_num_channels;
    
    // Calculate texture dimensions
    // Width is always MAX_TEXTURE_SIZE
    m_texture_width = MAX_TEXTURE_SIZE;
    // Height is rounded up to make square (or as close as possible)
    m_texture_rows = final_rows_per_channel;
    
    // Calculate actual window size in samples based on the adjusted texture size
    unsigned int actual_samples = m_texture_rows * MAX_TEXTURE_SIZE;
    m_window_size_seconds = static_cast<float>(actual_samples) / static_cast<float>(sample_rate);
    m_window_size_samples = actual_samples;
}

AudioTexture2DParameter * AudioRenderStageHistory2::create_audio_history_texture(GLuint active_texture_count) {
    // Create the texture parameter
    auto audio_history_texture = new AudioTexture2DParameter(m_audio_history_texture_name,
                            AudioParameter::ConnectionType::INPUT,
                            MAX_TEXTURE_SIZE, m_num_channels * m_texture_rows,
                            active_texture_count,
                            0, GL_NEAREST);
    
    // Initialize with zeros
    const unsigned int total_size = MAX_TEXTURE_SIZE * m_num_channels * m_texture_rows;
    float* temp_buffer = new float[total_size];
    std::fill(temp_buffer, temp_buffer + total_size, 0.0f);
    audio_history_texture->set_value(temp_buffer);
    delete[] temp_buffer;
    
    m_audio_history_texture = audio_history_texture;
    
    // Create uniform parameters for tape control
    m_tape_position = new AudioIntParameter("tape_position", AudioParameter::ConnectionType::INPUT);
    static_cast<AudioIntParameter*>(m_tape_position)->set_value(0);
    
    m_tape_speed = new AudioFloatParameter("tape_speed", AudioParameter::ConnectionType::INPUT);
    static_cast<AudioFloatParameter*>(m_tape_speed)->set_value(1.0f);
    
    m_tape_window_size_seconds = new AudioFloatParameter("tape_window_size_seconds", AudioParameter::ConnectionType::INPUT);
    static_cast<AudioFloatParameter*>(m_tape_window_size_seconds)->set_value(m_window_size_seconds);
    
    return audio_history_texture;
}

void AudioRenderStageHistory2::set_tape_position(const unsigned int tape_position) {
    m_current_tape_position_samples = tape_position;
    if (m_tape_position) {
        static_cast<AudioIntParameter*>(m_tape_position)->set_value(static_cast<int>(tape_position));
    }
}

void AudioRenderStageHistory2::set_tape_position(const float seconds_offset) {
    unsigned int samples_offset = static_cast<unsigned int>(seconds_offset * m_sample_rate);
    set_tape_position(samples_offset);
}

const unsigned int AudioRenderStageHistory2::get_tape_position() const {
    return m_current_tape_position_samples;
}

const float AudioRenderStageHistory2::get_tape_position_in_seconds() const {
    return static_cast<float>(m_current_tape_position_samples) / static_cast<float>(m_sample_rate);
}

void AudioRenderStageHistory2::set_tape_speed(const float speed) {
    m_current_tape_speed = speed;
    if (m_tape_speed) {
        static_cast<AudioFloatParameter*>(m_tape_speed)->set_value(speed);
    }
}

const float AudioRenderStageHistory2::get_tape_speed() const {
    return m_current_tape_speed;
}

std::vector<AudioParameter*> AudioRenderStageHistory2::get_uniform_parameters() {
    std::vector<AudioParameter*> params;
    if (m_tape_position) {
        params.push_back(m_tape_position);
    }
    if (m_tape_speed) {
        params.push_back(m_tape_speed);
    }
    if (m_tape_window_size_seconds) {
        params.push_back(m_tape_window_size_seconds);
    }
    return params;
}

void AudioRenderStageHistory2::update_audio_history_texture() {
    // TODO: Implement tape playback to texture update
    // Steps needed:
    // 1. Check if texture is created
    // 2. Check if tape is assigned (lock weak_ptr)
    // 3. Calculate read window based on current position and window size
    // 4. Read audio data from tape using playback()
    // 5. Convert from channel-major order to texture format (matching AudioRenderStageHistory format)
    // 6. Update texture with converted data
    // 7. Advance tape position based on playback speed (if speed != 0)
    // 8. Update uniform parameters with new position
    
    if (!m_audio_history_texture) {
        return; // Texture not created yet
    }
    
    // TODO: Implement tape playback logic here
    // For now, fill with zeros if no tape assigned
    auto tape = m_tape.lock();
    if (!tape) {
        const unsigned int total_size = MAX_TEXTURE_SIZE * m_num_channels * m_texture_rows;
        std::vector<float> zero_data(total_size, 0.0f);
        static_cast<AudioTexture2DParameter*>(m_audio_history_texture)->set_value(zero_data.data());
        return;
    }
    
    // TODO: Implement the rest of the playback logic
}