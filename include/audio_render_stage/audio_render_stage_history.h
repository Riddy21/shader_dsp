#pragma once
#ifndef AUDIO_RENDER_STAGE_HISTORY_H
#define AUDIO_RENDER_STAGE_HISTORY_H

#include <iostream>
#include <vector>
#include <GL/glew.h>

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

    void save_stream_to_history(const float * audio_stream_data);

    const std::vector<float> get_history_data();

    void update_audio_history_texture();

    std::string get_history_texture_name() { return "audio_history_texture"; }

private:
    std::vector<std::vector<float>> m_history_buffer;
    AudioParameter * m_audio_history_texture;
    const unsigned int m_num_channels;
    const unsigned int m_sample_rate;
    const unsigned int m_frames_per_buffer;
    const unsigned int m_texture_rows;
};

#endif // AUDIO_RENDER_STAGE_HISTORY_H