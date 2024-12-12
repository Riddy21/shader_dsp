
#pragma once
#ifndef AUDIO_MULTITRACK_JOIN_RENDER_STAGE_H
#define AUDIO_MULTITRACK_JOIN_RENDER_STAGE_H

#include "audio_render_stage.h"

class AudioMultitrackJoinRenderStage : public AudioRenderStage {
public:
    AudioMultitrackJoinRenderStage(const unsigned int frames_per_buffer,
                                   const unsigned int sample_rate,
                                   const unsigned int num_channels,
                                   const std::string& fragment_shader_path = "build/shaders/multitrack_join_render_stage.glsl",
                                   const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports);

    static const std::vector<std::string> default_frag_shader_imports;

    ~AudioMultitrackJoinRenderStage();
};

#endif // AUDIO_MULTITRACK_JOIN_RENDER_STAGE_H