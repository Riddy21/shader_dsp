#pragma once
#ifndef AUDIO_RENDER_STAGE_HISTORY_H
#define AUDIO_RENDER_STAGE_HISTORY_H

#include <iostream>
#include <vector>
#include <optional>
#include <memory>
#include <fstream>
#include <cstring>

#include "audio_parameter/audio_texture2d_parameter.h"
#include "audio_output/audio_wav.h"

#define MAX_TEXTURE_SIZE 4096

class AudioRenderStageHistory {
public:
    AudioRenderStageHistory(const unsigned int history_size,
                            const unsigned int frames_per_buffer,
                            const unsigned int sample_rate,
                            const unsigned int num_channels);

    AudioTexture2DParameter * create_audio_history_texture(GLuint m_active_texture_count);

    AudioParameter * get_audio_history_texture() { return m_audio_history_texture; }

    void shift_history_buffer();

    void save_stream_to_history(const float * audio_stream_data);

    const std::vector<float> get_history_data();

    void update_audio_history_texture();

    void clear_history_buffer();

    std::string get_history_texture_name() { return "audio_history_texture"; }

private:
    std::vector<std::vector<float>> m_history_buffer;
    AudioParameter * m_audio_history_texture;
    const unsigned int m_num_channels;
    const unsigned int m_sample_rate;
    const unsigned int m_frames_per_buffer;
    const unsigned int m_texture_rows;
};

// Forward declare for use in weak_ptr before full definition
class AudioTape;

class AudioRenderStageHistory2 {
public:
    AudioRenderStageHistory2(const unsigned int frames_per_buffer,
                             const unsigned int sample_rate,
                             const unsigned int num_channels,
                             const float history_buffer_size_seconds = 2.0f); // History buffer size in seconds of data stored in texture

    // Create all parameters (texture and uniform parameters)
    void create_parameters(GLuint& active_texture_count);
    
    // Get all parameters (texture and uniform parameters)
    std::vector<AudioParameter*> get_parameters() const;

    void set_tape(std::weak_ptr<AudioTape> tape) { m_tape = tape; }
    std::weak_ptr<AudioTape> get_tape() { return m_tape; }

    void set_tape_position(const unsigned int tape_position); 
    void set_tape_position(const float seconds_offset);
    const unsigned int get_tape_position() const;
    const float get_tape_position_in_seconds() const;

    void set_tape_speed(const float speed);
    
    // Get speed in ratio (1.0 = normal speed, 2.0 = double speed, etc.)
    const float get_tape_speed_ratio() const;
    
    // Get speed in samples per second
    const float get_tape_speed_samples_per_second() const;
    
    // Get speed in samples per buffer (can be negative for reverse playback)
    const int get_tape_speed_samples_per_buffer() const;
    
    // Get window size in samples (reads from parameter)
    const unsigned int get_window_size_samples() const;
    // Get window size in seconds (calculated from samples parameter)
    const float get_window_size_seconds() const;
    
    // Get window offset in samples (reads from parameter)
    const unsigned int get_window_offset_samples() const;
    // Get window offset in seconds (calculated from samples parameter)
    const float get_window_offset_seconds() const;

    // Tape control functions
    void stop_tape(); // Manually stop the tape
    void start_tape(); // Manually start the tape (resume from current position)
    bool is_tape_stopped() const; // Check if tape is stopped
    bool is_tape_at_beginning() const; // Check if tape is at the beginning
    bool is_tape_at_end() const; // Check if tape is at the end
    
    // Loop control functions
    void set_tape_loop(bool loop); // Enable or disable tape looping (default: false)
    bool is_tape_loop_enabled() const; // Check if tape looping is enabled

    // TODO: Implement incrementally updating the texture with tape playback data
    // When paused (speed = 0) it will stop updating position
    void update_audio_history_texture(const unsigned int time);

    std::string get_audio_history_texture_name() { return m_audio_history_texture_name; }
    
private:
    AudioParameter * m_audio_history_texture;
    AudioParameter * m_tape_position;
    AudioParameter * m_tape_window_size_samples; // Will communicate the window size in samples to the tape, so thats the maximum amount of samples that can be played at once
    AudioParameter * m_tape_speed; // Will communicate the speed of the tape to the tape, so thats the speed of the tape
    AudioParameter * m_tape_window_offset_samples; // Will communicate the current offset of the audio history texture in the window (in samples) from the start of the window
    AudioParameter * m_tape_stopped; // Flag indicating if tape is stopped (1 = stopped, 0 = playing)
    AudioParameter * m_tape_loop; // Flag indicating if tape should loop (1 = loop enabled, 0 = loop disabled, default = 0)

    std::weak_ptr<AudioTape> m_tape; // Weak reference to tape

    const unsigned int m_frames_per_buffer;
    const unsigned int m_sample_rate;
    const unsigned int m_num_channels;

    // Texture configuration (needed for texture creation)
    unsigned int m_texture_width;
    unsigned int m_texture_height;
    unsigned int m_texture_rows_per_channel;
    unsigned int m_window_size_samples;

    const std::string m_audio_history_texture_name = "audio_history_texture";
    
    // Track last time value for delta-based updates
    unsigned int m_last_time = 0;

    void set_window_offset_samples(const unsigned int window_offset_samples);

    bool is_audio_texture_data_outdated();
    const unsigned int get_window_offset_samples_for_tape_data() const;
    
    // Helper functions for update_audio_history_texture
    int calculate_time_delta(const unsigned int time);
    int calculate_backwards_time_delta(const unsigned int time);
    bool should_clamp_position(int samples_to_advance, unsigned int current_position, unsigned int tape_size);
    void clamp_position(int samples_to_advance, unsigned int current_position, unsigned int tape_size);
    void update_texture_if_needed(std::shared_ptr<AudioTape> tape);
};

class AudioTape {
friend class AudioRenderStageHistory2;
public:
    AudioTape(const unsigned int frames_per_buffer,
              const unsigned int sample_rate,
              const unsigned int num_channels, 
              const std::optional<unsigned int> tape_size = std::nullopt);

    // Static factory method to load AudioTape from a WAV file
    // Returns a shared_ptr to the created AudioTape, or nullptr on error
    // start_seconds: Optional start time in seconds (defaults to 0.0)
    // end_seconds: Optional end time in seconds (defaults to end of file)
    static std::shared_ptr<AudioTape> load_from_wav_file(const std::string& audio_filepath,
                                                          const unsigned int frames_per_buffer,
                                                          const unsigned int sample_rate,
                                                          const std::optional<float> start_seconds = std::nullopt,
                                                          const std::optional<float> end_seconds = std::nullopt);

    // Will record the audio data in one frames per buffer chunks
    // THe audio stream data should be frames per buffer * num channels long, in channel major order
    void record(const float * audio_stream_data);
    void record(const float * audio_stream_data, unsigned int samples_offset);
    void record(const float * audio_stream_data, float seconds_offset);

    // Will return the size of one frame in samples
    // The returned data will be frames per buffer * num channels long, in channel major order
    const std::vector<float> playback(const bool interleaved = false) const;
    const std::vector<float> playback(unsigned int samples_offset, const bool interleaved = false) const;
    const std::vector<float> playback(float seconds_offset, const bool interleaved = false) const;
    const std::vector<float> playback(unsigned int num_frames, unsigned int samples_offset, const bool interleaved = false) const;
    const std::vector<float> playback(unsigned int num_frames, float seconds_offset, const bool interleaved = false) const;

    void clear();
    // Number of samples stored per channel
    const unsigned int size() const { return m_data.empty() ? 0u : static_cast<unsigned int>(m_data[0].size()); }
    // Seconds of audio available per channel
    const float size_in_seconds() const { return static_cast<float>(size()) / static_cast<float>(m_sample_rate); }
    // Number of channels
    const unsigned int num_channels() const { return m_num_channels; }
    // Sample rate
    const unsigned int sample_rate() const { return m_sample_rate; }

private:
    // Playback for render stage history - outputs directly in texture format
    // Format: [ch0_row0][zeros][ch1_row0][zeros][ch0_row1][zeros][ch1_row1][zeros]...
    // Returns texture_width * texture_height samples where texture_height = num_channels * texture_rows_per_channel * 2
    // Only accessible by AudioRenderStageHistory2 (friend class)
    const std::vector<float> playback_for_render_stage_history(
        unsigned int window_size_samples,
        unsigned int samples_offset,
        unsigned int texture_width,
        unsigned int texture_rows_per_channel) const;

    using ChannelData = std::vector<float>; // contiguous per-channel time-series
    std::vector<ChannelData> m_data; // size = m_num_channels, each vector length = samples over time

    const unsigned int m_frames_per_buffer;
    const unsigned int m_sample_rate;
    const unsigned int m_num_channels;

    // Current position of the tape in samples
    unsigned int m_current_record_position = 0;
    // Current playback position of the tape in samples
    unsigned int m_current_playback_position = 0;

    bool m_fixed_size;
};

#endif // AUDIO_RENDER_STAGE_HISTORY_H