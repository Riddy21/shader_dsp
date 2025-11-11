#include <iostream>
#include <vector>
#include <cstring>

#include "audio_parameter/audio_texture2d_parameter.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_render_stage/audio_tape_render_stage.h"
#include "audio_render_stage_plugins/audio_render_stage_history.h"
#include "audio_core/audio_tape.h"

const std::vector<std::string> AudioRecordRenderStage::default_frag_shader_imports = {
    "build/shaders/global_settings.glsl",
    "build/shaders/frag_shader_settings.glsl"
};

void AudioRecordRenderStage::render(unsigned int time) {

    unsigned int current_block = (time - m_record_start_time);

    auto * data = (float *)this->find_parameter("stream_audio_texture")->get_value();

    if (m_recording) {
        unsigned int record_time = current_block + m_record_position;

        // If the tape is smaller than the record time, then resize the tape
        if (m_tape.size() <= record_time * frames_per_buffer * num_channels) {
            m_tape.resize((record_time + 1) * frames_per_buffer * num_channels); // Fill with 0s
        }

        // Copy the audio data to the tape
        std::copy(data, data + frames_per_buffer * num_channels, m_tape.begin() + record_time * frames_per_buffer * num_channels);
    }

    AudioRenderStage::render(time);
}

void AudioRecordRenderStage::record(unsigned int record_position) {
    m_recording = true;
    m_record_start_time = m_time;
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

    auto playback_texture = new AudioTexture2DParameter("playback_texture",
                                                        AudioParameter::ConnectionType::INPUT,
                                                        frames_per_buffer, num_channels * M_TAPE_SIZE, // Store 2 seconds of sound at a time
                                                        m_active_texture_count++,
                                                        0,
                                                        GL_NEAREST);
    auto playback_data = new float[frames_per_buffer * num_channels * M_TAPE_SIZE];
    playback_texture->set_value(playback_data);
    delete[] playback_data;

    auto play_position = new AudioIntParameter("play_position",
                                               AudioParameter::ConnectionType::INPUT);
    play_position->set_value(0);

    auto time_at_start = new AudioIntParameter("time_at_start",
                                               AudioParameter::ConnectionType::INPUT);
    time_at_start->set_value(0);

    auto play = new AudioBoolParameter("play",
                                          AudioParameter::ConnectionType::INPUT);
    play->set_value(false);


    if (!this->add_parameter(playback_texture)) {
        std::cerr << "Failed to add playback_texture" << std::endl;
    }
    if (!this->add_parameter(play_position)) {
        std::cerr << "Failed to add play_position" << std::endl;
    }
    if (!this->add_parameter(time_at_start)) {
        std::cerr << "Failed to add time_at_start" << std::endl;
    }
    if (!this->add_parameter(play)) {
        std::cerr << "Failed to add play" << std::endl;
    }
}

void AudioPlaybackRenderStage::load_tape(const Tape & tape) {
    m_tape_ptr = &tape;
}

void AudioPlaybackRenderStage::play(unsigned int play_position) {
    printf("Playing at position %d\n", play_position);
    this->find_parameter("play_position")->set_value(play_position);
    this->find_parameter("play")->set_value(true);
}

void AudioPlaybackRenderStage::stop() {
    printf("Stopping playback\n");
    this->find_parameter("play")->set_value(false);
}

bool AudioPlaybackRenderStage::is_playing() {
    return *(bool *)this->find_parameter("play")->get_value();
}

const unsigned int AudioPlaybackRenderStage::get_current_tape_position(const unsigned int time) {
    const unsigned int play_position = *(unsigned int *)this->find_parameter("play_position")->get_value();
    const unsigned int time_at_start = *(unsigned int *)this->find_parameter("time_at_start")->get_value();

    return (play_position + time - time_at_start) * frames_per_buffer * num_channels;
}

void AudioPlaybackRenderStage::load_tape_data_to_texture(const Tape & tape, const unsigned int offset) {
    // If there is no data in the tape, then return
    if (tape.size() == 0) {
        return;
    }

    // Get the right segment of tape
    float * buffered_data = new float[frames_per_buffer * num_channels * M_TAPE_SIZE]();

    // Copy the data to the buffer
    if (offset + frames_per_buffer * num_channels * M_TAPE_SIZE > tape.size()) {
        std::copy(tape.begin() + offset, tape.end(), buffered_data);
    } else {
        std::copy(tape.begin() + offset, tape.begin() + offset + frames_per_buffer * num_channels * M_TAPE_SIZE, buffered_data);
    }

    this->find_parameter("playback_texture")->set_value(buffered_data);

    delete[] buffered_data;
}

void AudioPlaybackRenderStage::render(const unsigned int time) {
    // Detect play state transition and preload the correct chunk for a seamless start
    const bool now_playing = is_playing();
    const unsigned int chunk_size = frames_per_buffer * num_channels * M_TAPE_SIZE;

    if (now_playing && !m_playing) {
        // Start playback on this frame: capture start time and load the tape chunk
        this->find_parameter("time_at_start")->set_value(time);

        if (m_tape_ptr != nullptr) {
            const unsigned int start_offset = get_current_tape_position(time);
            const unsigned int aligned_offset = start_offset - (start_offset % chunk_size);
            load_tape_data_to_texture(*m_tape_ptr, aligned_offset);
        }
    }

    m_playing = now_playing;

    if (now_playing) {
        const unsigned int offset = get_current_tape_position(time);

        // Keep streaming while there is audio left
        if (m_tape_ptr != nullptr && offset < m_tape_ptr->size()) {
            if (offset % chunk_size == 0) {
                load_tape_data_to_texture(*m_tape_ptr, offset);
            }
        } else {
            stop();
        }
    }

    AudioRenderStage::render(time);
}