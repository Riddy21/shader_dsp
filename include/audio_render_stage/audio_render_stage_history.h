#pragma once
#ifndef AUDIO_RENDER_STAGE_HISTORY_H
#define AUDIO_RENDER_STAGE_HISTORY_H

#include <iostream>
#include <vector>
#include <optional>
#include <memory>

#include "audio_parameter/audio_texture2d_parameter.h"

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

    AudioTexture2DParameter * create_audio_history_texture(GLuint m_active_texture_count);
    AudioParameter * get_audio_history_texture() { return m_audio_history_texture; }

    void set_tape(std::weak_ptr<AudioTape> tape) { m_tape = tape; }
    std::weak_ptr<AudioTape> get_tape() { return m_tape; }

    void set_tape_position(const unsigned int tape_position); 
    void set_tape_position(const float seconds_offset);
    const unsigned int get_tape_position() const;
    const float get_tape_position_in_seconds() const;

    void set_tape_speed(const float speed);
    const float get_tape_speed() const;
    
    // Get window size in seconds
    const float get_window_size_seconds() const { return m_window_size_seconds; }
    // Get window size in samples
    const unsigned int get_window_size_samples() const { return m_window_size_samples; }

    // TODO: Implement incrementally updating the texture with tape playback data
    // When paused (speed = 0) it will stop updating position
    void update_audio_history_texture();

    std::string get_audio_history_texture_name() { return m_audio_history_texture_name; }
    
    // Get all uniform parameters needed for the render stage
    // Returns a vector with: [tape_position, tape_speed, tape_window_size_seconds]
    std::vector<AudioParameter*> get_uniform_parameters();

private:
    AudioParameter * m_audio_history_texture;
    AudioParameter * m_tape_position;
    AudioParameter * m_tape_window_size_seconds; // Will communicate the window size in seconds to the tape, so thats the maximum amount of seconds seconds that can be played at once
    AudioParameter * m_tape_speed; // Will communicate the speed of the tape to the tape, so thats the speed of the tape

    std::weak_ptr<AudioTape> m_tape; // Weak reference to tape

    const unsigned int m_frames_per_buffer;
    const unsigned int m_sample_rate;
    const unsigned int m_num_channels;

    // Texture configuration
    unsigned int m_texture_width;  // Always MAX_TEXTURE_SIZE
    unsigned int m_texture_rows;   // Number of rows needed (per channel)
    float m_window_size_seconds;   // Window size in seconds
    unsigned int m_window_size_samples; // Window size in samples
    
    // Current playback state
    unsigned int m_current_tape_position_samples;
    float m_current_tape_speed;

    const std::string m_audio_history_texture_name = "audio_history_texture";
};

class AudioTape {
friend class AudioRenderStageHistory2;
public:
    AudioTape(const unsigned int frames_per_buffer,
              const unsigned int sample_rate,
              const unsigned int num_channels, 
              const std::optional<unsigned int> tape_size = std::nullopt);

    // Will record the audio data in one frames per buffer chunks
    // THe audio stream data should be frames per buffer * num channels long, in channel major order
    void record(const float * audio_stream_data, std::optional<unsigned int> samples_offset = std::nullopt);
    void record(const float * audio_stream_data, std::optional<float> seconds_offset = std::nullopt);

    // Will return the size of one frame in samples
    // The returned data will be frames per buffer * num channels long, in channel major order
    const std::vector<float> playback(std::optional<unsigned int> num_frames = std::nullopt,
                                      std::optional<unsigned int> samples_offset = std::nullopt,
                                      const bool interleaved = false) const;
    const std::vector<float> playback(std::optional<unsigned int> num_frames,
                                      std::optional<float> seconds_offset = std::nullopt,
                                      const bool interleaved = false) const;

    void clear();
    // Number of samples stored per channel
    const unsigned int size() const { return m_data.empty() ? 0u : static_cast<unsigned int>(m_data[0].size()); }
    // Seconds of audio available per channel
    const float size_in_seconds() const { return static_cast<float>(size()) / static_cast<float>(m_sample_rate); }

private:
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