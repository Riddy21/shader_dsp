#include <iostream>
#include <fstream>

#include "keyboard/key.h"
#include "audio_core/audio_renderer.h"
#include "audio_render_stage/audio_file_generator_render_stage.h"

PianoKey::PianoKey(const unsigned char key) : Key(key) {
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();

    AudioGeneratorRenderStage * audio_generator;

    //audio_generator = new AudioGeneratorRenderStage(512, 44100, 2, "build/shaders/sawtooth_generator_render_stage.glsl");
    //audio_generator = new AudioGeneratorRenderStage(512, 44100, 2, "build/shaders/triangle_generator_render_stage.glsl");
    audio_generator = new AudioGeneratorRenderStage(512, 44100, 1, "build/shaders/square_generator_render_stage.glsl");
    //audio_generator = new AudioGeneratorRenderStage(512, 44100, 2, "build/shaders/sine_generator_render_stage.glsl");
    //audio_generator = new AudioGeneratorRenderStage(512, 44100, 2, "build/shaders/static_generator_render_stage.glsl");
    //audio_generator = new AudioFileGeneratorRenderStage(512, 44100, 2, "media/test.wav");

    auto gid = audio_generator->gid;

    m_audio_generator = audio_generator;

    m_gain_param = m_audio_generator->find_parameter("gain");
    m_tone_param = m_audio_generator->find_parameter("tone");
    m_play_position_param = m_audio_generator->find_parameter("play_position");
    m_stop_position_param = m_audio_generator->find_parameter("stop_position");
    m_time_param = audio_renderer.find_global_parameter("global_time");
    m_play_param = m_audio_generator->find_parameter("play");

    set_key_down_callback([this]() {
        m_play_position_param->set_value(m_time_param->get_value());
        m_play_param->set_value(true);
    });

    set_key_up_callback([this]() {
        m_stop_position_param->set_value(m_time_param->get_value());
        m_play_param->set_value(false);
    });
}

void PianoKey::set_gain(const float gain) {
    m_gain = gain;
    m_gain_param->set_value(gain);
}

void PianoKey::set_tone(const float tone) {
    m_tone = tone;
    m_tone_param->set_value(tone);
}