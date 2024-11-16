#include <vector>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>

#include "audio_output/audio_wav.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_render_stage/audio_generator_render_stage.h"

AudioGeneratorRenderStage::AudioGeneratorRenderStage(const unsigned int frames_per_buffer,
                                                     const unsigned int sample_rate,
                                                     const unsigned int num_channels,
                                                     const char * fragment_shader_path)
    : AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_path) {

        auto play_position_parameter =
            new AudioIntParameter("play_position",
                                  AudioParameter::ConnectionType::INPUT);
        play_position_parameter->set_value(new int(0));

        auto tone_parameter =
            new AudioFloatParameter("tone",
                                  AudioParameter::ConnectionType::INPUT);
        tone_parameter->set_value(new float(1.0f));

        auto gain_parameter =
            new AudioFloatParameter("gain",
                                  AudioParameter::ConnectionType::INPUT);
        gain_parameter->set_value(new float(0.0f));

        auto buffer_size =
            new AudioIntParameter("buffer_size",
                      AudioParameter::ConnectionType::INITIALIZATION);
        buffer_size->set_value(new int(m_frames_per_buffer*m_num_channels));

        if (!this->add_parameter(tone_parameter)) {
            std::cerr << "Failed to add tone_parameter" << std::endl;
        }
        if (!this->add_parameter(gain_parameter)) {
            std::cerr << "Failed to add play_parameter" << std::endl;
        }
        if (!this->add_parameter(play_position_parameter)) {
            std::cerr << "Failed to add play_position_parameter" << std::endl;
        }
        if (!this->add_parameter(buffer_size)) {
            std::cerr << "Failed to add buffer_size" << std::endl;
        }
}
