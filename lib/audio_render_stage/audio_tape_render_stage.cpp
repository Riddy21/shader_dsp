#include <iostream>
#include <vector>

#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_render_stage/audio_tape_render_stage.h"

const std::vector<std::string> AudioRecordRenderStage::default_frag_shader_imports = {
    "build/shaders/global_settings.glsl",
    "build/shaders/frag_shader_settings.glsl"
};

AudioRecordRenderStage::AudioRecordRenderStage(const unsigned int frames_per_buffer,
                                               const unsigned int sample_rate,
                                               const unsigned int num_channels,
                                               const std::string & fragment_shader_path,
                                               const std::vector<std::string> & frag_shader_imports)
    : AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {

    auto record_time = 
        new AudioIntParameter("record_position",
                              AudioParameter::ConnectionType::INPUT);
    record_time->set_value(0);

    auto recording = 
        new AudioBoolParameter("recording",
                              AudioParameter::ConnectionType::INPUT);
    recording->set_value(false);

    if (!this->add_parameter(record_time)) {
        std::cerr << "Failed to add record_time" << std::endl;
    }
    if (!this->add_parameter(recording)) {
        std::cerr << "Failed to add recording" << std::endl;
    }
}

void AudioRecordRenderStage::render(unsigned int time) {

    // Copy the audio data to the tape
    static int prev_time = 0;

    auto * data = (float *)this->find_parameter("stream_audio_texture")->get_value();

    unsigned int record_time = *(int *)this->find_parameter("record_position")->get_value();

    bool recording = *(bool *)this->find_parameter("recording")->get_value();

    if (recording && time != prev_time) {
        // FIXME: Current time is not set right
        // If the tape is smaller than the record time, then resize the tape
        if (m_tape.size() <= record_time * m_frames_per_buffer * m_num_channels) {
            m_tape.resize((record_time + 1) * m_frames_per_buffer * m_num_channels); // Fill with 0s
        }

        printf("Tape size: %d\n", m_tape.size());

        // Copy the audio data to the tape
        std::copy(data, data + m_frames_per_buffer * m_num_channels, m_tape.begin() + record_time);
    }

    prev_time = time;

    AudioRenderStage::render(time);

    //printf("Tape size: %d\n", m_tape.size());
}

// TODO: Implement copy and paste functionality