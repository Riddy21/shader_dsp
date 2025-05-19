
#pragma once
#ifndef AUDIO_MULTITRACK_JOIN_RENDER_STAGE_H
#define AUDIO_MULTITRACK_JOIN_RENDER_STAGE_H

#include <queue>
#include <unordered_set>
#include "audio_core/audio_render_stage.h"
#include "audio_core/audio_parameter.h"

class AudioMultitrackJoinRenderStage : public AudioRenderStage {
public:
    AudioMultitrackJoinRenderStage(const unsigned int frames_per_buffer,
                                   const unsigned int sample_rate,
                                   const unsigned int num_channels,
                                   const unsigned int num_tracks,
                                   const std::string& fragment_shader_path = "build/shaders/multitrack_join_render_stage.glsl",
                                   const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports);

    ~AudioMultitrackJoinRenderStage() {};

    static const std::vector<std::string> default_frag_shader_imports;

private:
    const std::vector<AudioParameter *> get_stream_interface() override;
    bool release_stream_interface(AudioRenderStage * prev_stage) override;

    std::queue<AudioParameter *> m_free_textures;
    std::unordered_set<AudioParameter *> m_used_textures;

};

#endif // AUDIO_MULTITRACK_JOIN_RENDER_STAGE_H