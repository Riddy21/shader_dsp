#include <iostream>
#include <limits>

#include "audio_render_stage/audio_tape_render_stage.h"
#include "audio_render_stage_plugins/audio_render_stage_history.h"
#include "audio_core/audio_tape.h"

const std::vector<std::string> AudioRecordRenderStage::default_frag_shader_imports = {
    "build/shaders/global_settings.glsl",
    "build/shaders/frag_shader_settings.glsl"
};

AudioRecordRenderStage::AudioRecordRenderStage(const std::string & stage_name,
                                               const unsigned int frames_per_buffer,
                                               const unsigned int sample_rate,
                                               const unsigned int num_channels,
                                               const std::string& fragment_shader_path,
                                               const std::vector<std::string> & frag_shader_imports)
    : AudioRenderStage(stage_name, frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {
    
    // Making a tape for this render stage
    m_tape_new = std::make_shared<AudioTape>(frames_per_buffer, sample_rate, num_channels);
}

void AudioRecordRenderStage::render(unsigned int time) {

    unsigned int current_block = (time - m_record_start_time);

    auto * data = (float *)this->find_parameter("stream_audio_texture")->get_value();


    if (m_recording) {

        unsigned int record_time = current_block + m_record_position;

        m_tape_new->record(data, record_time * frames_per_buffer);
    }

    AudioRenderStage::render(time);
}

void AudioRecordRenderStage::record(unsigned int record_position) {
    m_recording = true;

    // FIXME: Find a cleaner fix for this
    if (m_time == std::numeric_limits<unsigned int>::max()) {
        m_record_start_time = 0;
    } else {
        m_record_start_time = m_time;
    }
    m_record_position = record_position;
    printf("Recording started at time %d\n", m_record_start_time);
}

void AudioRecordRenderStage::stop() {
    m_recording = false;
    printf("Recording stopped at time %d\n", m_time);
}

// TODO: Implement copy and paste functionality

// TODO: Set functionality to save this data and reload it on shutdown

const std::vector<std::string> AudioPlaybackRenderStage::default_frag_shader_imports = {
    "build/shaders/global_settings.glsl",
    "build/shaders/frag_shader_settings.glsl"
};

AudioPlaybackRenderStage::AudioPlaybackRenderStage(const unsigned int frames_per_buffer,
                                                   const unsigned int sample_rate,
                                                   const unsigned int num_channels,
                                                   const std::string& fragment_shader_path,
                                                   const std::vector<std::string> & frag_shader_imports)
        : AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {

    m_history = std::make_unique<AudioRenderStageHistory2>(frames_per_buffer, sample_rate, num_channels, 1.0f); // Adjust size as needed

    if (!this->register_plugin(m_history.get())) {
        std::cerr << "Failed to register history plugin" << std::endl;
    }

    // Set the default
    m_history->set_tape_speed(1.0f);
    m_history->set_tape_position(0u);
}

void AudioPlaybackRenderStage::load_tape(const std::weak_ptr<AudioTape> tape) {
    m_tape_ptr_new = tape.lock();
    m_history->set_tape(tape);

    m_history->update_window();
}

void AudioPlaybackRenderStage::play(unsigned int play_position) {
    printf("Playing at position %d\n", play_position);

    m_history->set_tape_position(play_position * frames_per_buffer);
    m_history->start_tape();

    m_history->update_window();
}

void AudioPlaybackRenderStage::stop() {
    printf("Stopping playback\n");

    m_history->stop_tape();
}

bool AudioPlaybackRenderStage::is_playing() {
    return !m_history->is_tape_stopped();
}

const unsigned int AudioPlaybackRenderStage::get_current_tape_position(const unsigned int time) {
    // Get the current tape position from the history plugin
    // The position is in samples (per channel)
    int position = m_history->get_tape_position();
    // Ensure non-negative (history plugin can return negative values in some edge cases)
    return (position >= 0) ? static_cast<unsigned int>(position) : 0u;
}

void AudioPlaybackRenderStage::render(const unsigned int time) {
    unsigned int current_time = m_time;

    AudioRenderStage::render(time);

    if (current_time != time) {
        m_history->update_audio_history_texture();
    }
}
