#include <iostream>

#include "key.h"
#include "audio_renderer.h"

PianoKey::PianoKey(const unsigned char key, const char * audio_file_path) : Key(key) {
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();

    auto audio_generator = std::make_unique<AudioGeneratorRenderStage>(512, 44100, 2, audio_file_path);

    auto gid = audio_generator->gid;

    bool result = audio_renderer.add_render_stage(std::move(audio_generator));

    if (!result) {
        std::cerr << "Failed to add render stage." << std::endl;
        return;
    }

    m_audio_generator = (AudioGeneratorRenderStage *)audio_renderer.find_render_stage(gid);

    m_gain_param = m_audio_generator->find_parameter("gain");
    m_tone_param = m_audio_generator->find_parameter("tone");
    m_play_param = m_audio_generator->find_parameter("play_position");
    m_time_param = m_audio_generator->find_parameter("time");
}

void PianoKey::key_down() {
    m_play_param->set_value(m_time_param->get_value());
    m_gain_param->set_value(new float(m_gain));
    m_tone_param->set_value(new float(m_tone));
}

void PianoKey::key_up() {
    m_gain_param->set_value(new float(0.0));
    m_tone_param->set_value(new float(0.0f));
}