#pragma once
#ifndef AUDIO_TAPE_RENDER_STAGE_H
#define AUDIO_TAPE_RENDER_STAGE_H

#include "audio_render_stage/audio_render_stage.h"

class AudioRecordRenderStage : public AudioRenderStage {
public:
    AudioRecordRenderStage(const unsigned int frames_per_buffer,
                           const unsigned int sample_rate,
                           const unsigned int num_channels,
                           const std::string& fragment_shader_path = "build/shaders/record_render_stage.glsl",
                           const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports);

    static const std::vector<std::string> default_frag_shader_imports;

    ~AudioRecordRenderStage() {};

private:
    void render(unsigned int time) override;

    std::vector<float> m_tape;
};

#endif // AUDIO_TAPE_RENDER_STAGE_H