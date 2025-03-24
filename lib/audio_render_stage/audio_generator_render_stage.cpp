#include <vector>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>

#include "audio_output/audio_wav.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_parameter/audio_uniform_array_parameter.h"
#include "audio_render_stage/audio_generator_render_stage.h"

const std::vector<std::string> AudioSingleShaderGeneratorRenderStage::default_frag_shader_imports = {
    "build/shaders/global_settings.glsl",
    "build/shaders/frag_shader_settings.glsl",
    "build/shaders/generator_render_stage_settings.glsl",
    "build/shaders/adsr_envelope.glsl"
};

AudioSingleShaderGeneratorRenderStage::AudioSingleShaderGeneratorRenderStage(const unsigned int frames_per_buffer,
                                                     const unsigned int sample_rate,
                                                     const unsigned int num_channels,
                                                     const std::string & fragment_shader_path,
                                                     const std::vector<std::string> & frag_shader_imports)
    : AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {

    auto play_position_parameter =
        new AudioIntParameter("play_position",
                              AudioParameter::ConnectionType::INPUT);
    play_position_parameter->set_value(0);

    auto stop_position_parameter =
        new AudioIntParameter("stop_position",
                              AudioParameter::ConnectionType::INPUT);
    stop_position_parameter->set_value(-1);

    auto play_parameter =
        new AudioBoolParameter("play",
                              AudioParameter::ConnectionType::INPUT);
    play_parameter->set_value(false);

    auto tone_parameter =
        new AudioFloatParameter("tone",
                              AudioParameter::ConnectionType::INPUT);
    tone_parameter->set_value(MIDDLE_C);

    auto gain_parameter =
        new AudioFloatParameter("gain",
                              AudioParameter::ConnectionType::INPUT);
    gain_parameter->set_value(1.0f);

    if (!this->add_parameter(play_parameter)) {
        std::cerr << "Failed to add play_parameter" << std::endl;
    }
    if (!this->add_parameter(tone_parameter)) {
        std::cerr << "Failed to add tone_parameter" << std::endl;
    }
    if (!this->add_parameter(gain_parameter)) {
        std::cerr << "Failed to add gain_parameter" << std::endl;
    }
    if (!this->add_parameter(play_position_parameter)) {
        std::cerr << "Failed to add play_position_parameter" << std::endl;
    }
    if (!this->add_parameter(stop_position_parameter)) {
        std::cerr << "Failed to add stop_position_parameter" << std::endl;
    }

    // TODO: Envelope Parameters, Consider moving to a separate class
    auto attack_time_parameter =
        new AudioFloatParameter("attack_time",
                              AudioParameter::ConnectionType::INPUT);
    attack_time_parameter->set_value(0.1f);

    auto decay_time_parameter =
        new AudioFloatParameter("decay_time",
                              AudioParameter::ConnectionType::INPUT);
    decay_time_parameter->set_value(1.0f);

    auto sustain_level_parameter =
        new AudioFloatParameter("sustain_level",
                              AudioParameter::ConnectionType::INPUT);
    sustain_level_parameter->set_value(1.0f);

    auto release_time_parameter =
        new AudioFloatParameter("release_time",
                              AudioParameter::ConnectionType::INPUT);
    release_time_parameter->set_value(0.4f);

    if (!this->add_parameter(attack_time_parameter)) {
        std::cerr << "Failed to add attack_time_parameter" << std::endl;
    }
    if (!this->add_parameter(decay_time_parameter)) {
        std::cerr << "Failed to add decay_time_parameter" << std::endl;
    }
    if (!this->add_parameter(sustain_level_parameter)) {
        std::cerr << "Failed to add sustain_level_parameter" << std::endl;
    }
    if (!this->add_parameter(release_time_parameter)) {
        std::cerr << "Failed to add release_time_parameter" << std::endl;
    }
}

const std::vector<std::string> AudioGeneratorRenderStage::default_frag_shader_imports = {
    "build/shaders/global_settings.glsl",
    "build/shaders/frag_shader_settings.glsl",
    "build/shaders/multinote_generator_render_stage_settings.glsl",
    "build/shaders/adsr_envelope.glsl"
};

AudioGeneratorRenderStage::AudioGeneratorRenderStage(const unsigned int frames_per_buffer,
                                                     const unsigned int sample_rate,
                                                     const unsigned int num_channels,
                                                     const std::string & fragment_shader_path,
                                                     const std::vector<std::string> & frag_shader_imports)
    : AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {

    auto play_position_parameter =
        new AudioIntArrayParameter("play_positions",
                              AudioParameter::ConnectionType::INPUT,
                              MAX_NOTES_PLAYED_AT_ONCE);

    auto stop_position_parameter =
        new AudioIntArrayParameter("stop_positions",
                              AudioParameter::ConnectionType::INPUT,
                              MAX_NOTES_PLAYED_AT_ONCE);

    auto tone_parameter =
        new AudioFloatArrayParameter("tones",
                              AudioParameter::ConnectionType::INPUT,
                              MAX_NOTES_PLAYED_AT_ONCE);
    
    auto gain_parameter =
        new AudioFloatArrayParameter("gains",
                              AudioParameter::ConnectionType::INPUT,
                              MAX_NOTES_PLAYED_AT_ONCE);

    auto active_notes_parameter =
        new AudioIntParameter("active_notes",
                              AudioParameter::ConnectionType::INPUT);
                        
    // Set to 0s
    int* play_positions = new int[MAX_NOTES_PLAYED_AT_ONCE]();
    int* stop_positions = new int[MAX_NOTES_PLAYED_AT_ONCE]();
    float* tones = new float[MAX_NOTES_PLAYED_AT_ONCE]();
    float* gains = new float[MAX_NOTES_PLAYED_AT_ONCE]();

    play_position_parameter->set_value(play_positions);
    stop_position_parameter->set_value(stop_positions);
    tone_parameter->set_value(tones);
    gain_parameter->set_value(gains);
    active_notes_parameter->set_value(0);

    // Clean up allocated arrays
    delete[] play_positions;
    delete[] stop_positions;
    delete[] tones;
    delete[] gains;

    // Add the parameters
    if (!this->add_parameter(play_position_parameter)) {
        std::cerr << "Failed to add play_position_parameter" << std::endl;
    }
    if (!this->add_parameter(stop_position_parameter)) {
        std::cerr << "Failed to add stop_position_parameter" << std::endl;
    }
    if (!this->add_parameter(tone_parameter)) {
        std::cerr << "Failed to add tone_parameter" << std::endl;
    }
    if (!this->add_parameter(gain_parameter)) {
        std::cerr << "Failed to add gain_parameter" << std::endl;
    }
    if (!this->add_parameter(active_notes_parameter)) {
        std::cerr << "Failed to add active_notes_parameter" << std::endl;
    }

    auto attack_time_parameter =
        new AudioFloatParameter("attack_time",
                              AudioParameter::ConnectionType::INPUT);
    attack_time_parameter->set_value(0.1f);

    auto decay_time_parameter =
        new AudioFloatParameter("decay_time",
                              AudioParameter::ConnectionType::INPUT);
    decay_time_parameter->set_value(1.0f);

    auto sustain_level_parameter =
        new AudioFloatParameter("sustain_level",
                              AudioParameter::ConnectionType::INPUT);
    sustain_level_parameter->set_value(1.0f);

    auto release_time_parameter =
        new AudioFloatParameter("release_time",
                              AudioParameter::ConnectionType::INPUT);
    release_time_parameter->set_value(0.4f);

    if (!this->add_parameter(attack_time_parameter)) {
        std::cerr << "Failed to add attack_time_parameter" << std::endl;
    }
    if (!this->add_parameter(decay_time_parameter)) {
        std::cerr << "Failed to add decay_time_parameter" << std::endl;
    }
    if (!this->add_parameter(sustain_level_parameter)) {
        std::cerr << "Failed to add sustain_level_parameter" << std::endl;
    }
    if (!this->add_parameter(release_time_parameter)) {
        std::cerr << "Failed to add release_time_parameter" << std::endl;
    }
}

void AudioGeneratorRenderStage::play_note(const float tone, const float gain)
{
    auto active_notes = find_parameter("active_notes");
    auto play_positions = find_parameter("play_positions");
    auto stop_positions = find_parameter("stop_positions");
    auto tones = find_parameter("tones");
    auto gains = find_parameter("gains");

    // TODO: Modify the local copy of the parameters
    m_active_notes++;

    // TODO: Sync them with the shader
}

void AudioGeneratorRenderStage::stop_note(const float tone)
{
}