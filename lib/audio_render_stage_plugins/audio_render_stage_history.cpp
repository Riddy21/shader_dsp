#include "audio_render_stage_plugins/audio_render_stage_history.h"
#include "audio_core/audio_tape.h"
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
      m_tape_window_offset_samples(nullptr),
      m_tape_stopped(nullptr),
      m_tape_loop(nullptr) {
    
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
    
    // Create tape stopped flag parameter (1 = stopped, 0 = playing)
    m_tape_stopped = new AudioIntParameter("tape_stopped", AudioParameter::ConnectionType::INPUT);
    static_cast<AudioIntParameter*>(m_tape_stopped)->set_value(0); // Start as playing
    
    // Create tape loop flag parameter (1 = loop enabled, 0 = loop disabled, default = 0)
    m_tape_loop = new AudioIntParameter("tape_loop", AudioParameter::ConnectionType::INPUT);
    static_cast<AudioIntParameter*>(m_tape_loop)->set_value(0); // Default to no loop
    
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
    if (m_tape_stopped) params.push_back(m_tape_stopped);
    if (m_tape_loop) params.push_back(m_tape_loop);
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

void AudioRenderStageHistory2::stop_tape() {
    if (m_tape_stopped) {
        static_cast<AudioIntParameter*>(m_tape_stopped)->set_value(1);
    }
    // Set speed to 0 to stop playback
    set_tape_speed(0.0f);
}

void AudioRenderStageHistory2::start_tape() {
    if (m_tape_stopped) {
        static_cast<AudioIntParameter*>(m_tape_stopped)->set_value(0);
    }
    // Speed should be set separately by user, we just clear the stopped flag
}

bool AudioRenderStageHistory2::is_tape_stopped() const {
    if (m_tape_stopped) {
        return *static_cast<const int*>(m_tape_stopped->get_value()) != 0;
    }
    return false;
}

void AudioRenderStageHistory2::set_tape_loop(bool loop) {
    if (m_tape_loop) {
        static_cast<AudioIntParameter*>(m_tape_loop)->set_value(loop ? 1 : 0);
    }
}

bool AudioRenderStageHistory2::is_tape_loop_enabled() const {
    if (m_tape_loop) {
        return *static_cast<const int*>(m_tape_loop->get_value()) != 0;
    }
    return false;
}

bool AudioRenderStageHistory2::is_tape_at_beginning() const {
    return get_tape_position() == 0u;
}

bool AudioRenderStageHistory2::is_tape_at_end() const {
    auto tape = m_tape.lock();
    if (!tape) {
        return false;
    }
    return get_tape_position() >= tape->size();
}

void AudioRenderStageHistory2::update_audio_history_texture(const unsigned int time) {
    // Early returns for invalid states
    int time_delta = calculate_time_delta(time);
    if (time_delta == 0) {
        return; // Time hasn't changed, no update needed
    }
    
    // Check if tape is stopped - if so, don't update
    if (is_tape_stopped()) {
        return;
    }
    
    if (get_tape_speed_ratio() == 0.0f) {
        return; // Speed is 0, don't update
    }
    
    if (!m_audio_history_texture) {
        return; // Texture not created yet
    }
    
    auto tape = m_tape.lock();
    if (!tape) {
        return; // Tape not assigned
    }
    
    // Get current state
    const unsigned int current_position = get_tape_position();
    const int speed_samples_per_buffer = get_tape_speed_samples_per_buffer();
    const int samples_to_advance = time_delta * speed_samples_per_buffer;
    
    // Check if loop is enabled
    const bool loop_enabled = is_tape_loop_enabled();
    const unsigned int tape_size = tape->size();
    
    // Handle boundaries: wrap around if looping, otherwise clamp and stop
    if (should_clamp_position(samples_to_advance, current_position, tape_size)) {
        if (loop_enabled && tape_size > 0) {
            // Wrap around: calculate wrapped position
            unsigned int wrapped_position;
            if (samples_to_advance < 0) {
                // Moving backwards past start: wrap to end
                unsigned int overshoot = static_cast<unsigned int>(-samples_to_advance) - current_position;
                wrapped_position = (tape_size > overshoot) ? (tape_size - overshoot) : 0u;
            } else {
                // Moving forwards past end: wrap to start
                unsigned int overshoot = (current_position + static_cast<unsigned int>(samples_to_advance)) - tape_size;
                wrapped_position = overshoot % tape_size;
            }
            set_tape_position(wrapped_position);
            // Update texture after wrapping
            update_texture_if_needed(tape);
        } else {
            // No loop: clamp and stop
            clamp_position(samples_to_advance, current_position, tape_size);
            stop_tape();
            return;
        }
    } else {
        // Update texture if needed (before advancing position)
        update_texture_if_needed(tape);
        
        // Advance position based on time delta and speed
        unsigned int new_position = current_position + samples_to_advance;
        
        // Handle wrapping if loop is enabled and we've crossed a boundary
        if (loop_enabled) {
            if (samples_to_advance > 0 && new_position >= tape_size) {
                // Wrapped past end: wrap to start
                new_position = new_position % tape_size;
            } else if (samples_to_advance < 0 && new_position > tape_size) {
                // Wrapped past start (unsigned wraparound): wrap to end
                new_position = tape_size - (tape_size - new_position) % tape_size;
            }
        }
        
        set_tape_position(new_position);
        
        // Check if we've reached the end or beginning after advancing (only if not looping)
        if (!loop_enabled) {
            if (samples_to_advance > 0 && new_position >= tape_size) {
                stop_tape();
            } else if (samples_to_advance < 0 && new_position == 0u && current_position > 0u) {
                // Only stop if we were moving backwards and reached 0 (not if we were already at 0)
                stop_tape();
            }
        }
    }
}

int AudioRenderStageHistory2::calculate_time_delta(const unsigned int time) {
    // First frame: default increment to 1 buffer period
    if (m_last_time == 0) {
        m_last_time = time;
        return 1;
    }
    
    // Calculate signed delta to support backwards time movement
    int time_delta;
    if (time < m_last_time) {
        time_delta = calculate_backwards_time_delta(time);
    } else {
        time_delta = static_cast<int>(time - m_last_time);
    }
    
    m_last_time = time;
    return time_delta;
}

int AudioRenderStageHistory2::calculate_backwards_time_delta(const unsigned int time) {
    const unsigned int diff = m_last_time - time;
    
    // Check if this is wraparound (very large difference) or normal backwards movement
    // If difference > half of unsigned int max, assume wraparound
    const unsigned int wraparound_threshold = std::numeric_limits<unsigned int>::max() / 2;
    
    if (diff > wraparound_threshold) {
        // Likely wraparound - use small negative delta to avoid huge jumps
        return -1;
    } else {
        // Normal backwards movement
        return -static_cast<int>(diff);
    }
}

bool AudioRenderStageHistory2::should_clamp_position(int samples_to_advance, 
                                                      unsigned int current_position, 
                                                      unsigned int tape_size) {
    // Moving backwards beyond start
    if (samples_to_advance < 0 && static_cast<unsigned int>(-samples_to_advance) > current_position) {
        return true;
    }
    
    // Moving forwards beyond end
    if (samples_to_advance > 0 && current_position + static_cast<unsigned int>(samples_to_advance) >= tape_size) {
        return true;
    }
    
    return false;
}

void AudioRenderStageHistory2::clamp_position(int samples_to_advance, 
                                               unsigned int current_position, 
                                               unsigned int tape_size) {
    if (samples_to_advance < 0) {
        set_tape_position(0u);
    } else {
        set_tape_position(tape_size);
    }
}

void AudioRenderStageHistory2::update_texture_if_needed(std::shared_ptr<AudioTape> tape) {
    if (!is_audio_texture_data_outdated()) {
        return;
    }
    
    const unsigned int window_offset_samples = get_window_offset_samples_for_tape_data();
    
    // Get tape data in texture format
    std::vector<float> texture_data = tape->playback_for_render_stage_history(
        get_window_size_samples(),
        window_offset_samples,
        m_texture_width,
        m_texture_rows_per_channel);
    
    // Update texture
    static_cast<AudioTexture2DParameter*>(m_audio_history_texture)->set_value(texture_data.data());
    
    // Update window offset parameter
    set_window_offset_samples(window_offset_samples);
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

void AudioRenderStageHistory2::set_tape(std::weak_ptr<AudioTape> tape) {
    m_tape = tape;
}

std::weak_ptr<AudioTape> AudioRenderStageHistory2::get_tape() {
    return m_tape;
}

