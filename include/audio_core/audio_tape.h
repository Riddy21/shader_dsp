#pragma once
#ifndef AUDIO_TAPE_H
#define AUDIO_TAPE_H

#include <iostream>
#include <vector>
#include <optional>
#include <memory>
#include <string>

// Forward declaration for friend class
class AudioRenderStageHistory2;

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
    
    // Export tape to WAV file
    // Returns true on success, false on failure
    bool export_to_wav_file(const std::string& output_filepath) const;

    // Get current record and playback positions
    const unsigned int get_current_record_position() const { return m_current_record_position; }
    const unsigned int get_current_playback_position() const { return m_current_playback_position; }

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

#endif // AUDIO_TAPE_H

