#pragma once
#ifndef AUDIO_RENDER_STAGE_HISTORY_H
#define AUDIO_RENDER_STAGE_HISTORY_H

#include <iostream>
#include <vector>
#include <optional>
#include <memory>

#include "audio_parameter/audio_texture2d_parameter.h"

// Forward declaration
class AudioTape;

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

    std::string get_history_texture_name() { return "audio_history_texture_old"; }

private:
    std::vector<std::vector<float>> m_history_buffer;
    AudioParameter * m_audio_history_texture;
    const unsigned int m_num_channels;
    const unsigned int m_sample_rate;
    const unsigned int m_frames_per_buffer;
    const unsigned int m_texture_rows;
};

class AudioRenderStageHistory2 {
public:
    AudioRenderStageHistory2(const unsigned int frames_per_buffer,
                             const unsigned int sample_rate,
                             const unsigned int num_channels,
                             const float history_buffer_size_seconds = 2.0f); // History buffer size in seconds of data stored in texture

    // Create all parameters (texture and uniform parameters)
    void create_parameters(GLuint active_texture_count);
    
    // Get all parameters (texture and uniform parameters)
    std::vector<AudioParameter*> get_parameters() const;

    void set_tape(std::weak_ptr<AudioTape> tape);
    std::weak_ptr<AudioTape> get_tape();

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

    // Update tape position based on time - must be called every frame
    void update_tape_position(const unsigned int time);
    
    // Check if the tape position is out of bounds of the current window
    bool is_outdated() const;
    
    // Update window - force update without any parameters, takes based on tape position
    void update_window();
    
    // TODO: Implement incrementally updating the texture with tape playback data
    // When paused (speed = 0) it will stop updating position
    // This function calls both update_tape_position() and update_window() for backward compatibility
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

    const std::string m_audio_history_texture_name = "tape_history_texture";
    
    // Track last time value for delta-based updates
    unsigned int m_last_time = 0;

    // Track pending speed change to ensure continuity when speed changes
    // Speed changes are deferred until after position advancement, so position advances
    // using the speed that was used for the previous frame's rendering
    std::optional<int> m_pending_speed_samples_per_buffer;

    void set_window_offset_samples(const unsigned int window_offset_samples);

    const unsigned int get_window_offset_samples_for_tape_data() const;
    
    // Helper functions for update_audio_history_texture
    int calculate_time_delta(const unsigned int time);
    int calculate_backwards_time_delta(const unsigned int time);
    bool should_clamp_position(int samples_to_advance, unsigned int current_position, unsigned int tape_size);
    void clamp_position(int samples_to_advance, unsigned int current_position, unsigned int tape_size);
    void advance_tape_position_with_delta(int time_delta); // Internal helper that advances position given a pre-calculated time delta
};

#endif // AUDIO_RENDER_STAGE_HISTORY_H

