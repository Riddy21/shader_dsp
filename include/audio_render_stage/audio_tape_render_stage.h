#pragma once
#ifndef AUDIO_TAPE_RENDER_STAGE_H
#define AUDIO_TAPE_RENDER_STAGE_H

#include "audio_core/audio_render_stage.h"
#include "audio_render_stage_plugins/audio_render_stage_history.h"
#include "audio_core/audio_tape.h"

typedef std::vector<float> Tape;
class AudioRecordRenderStage : public AudioRenderStage {
public:
    AudioRecordRenderStage(const unsigned int frames_per_buffer,
                           const unsigned int sample_rate,
                           const unsigned int num_channels,
                           const std::string& fragment_shader_path = "build/shaders/render_stage_frag.glsl",
                           const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports)
        : AudioRecordRenderStage("RecordStage-" + std::to_string(generate_id()), frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {}

    // Named constructor
    AudioRecordRenderStage(const std::string & stage_name,
                           const unsigned int frames_per_buffer,
                           const unsigned int sample_rate,
                           const unsigned int num_channels,
                           const std::string& fragment_shader_path = "build/shaders/render_stage_frag.glsl",
                           const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports);

    static const std::vector<std::string> default_frag_shader_imports;

    ~AudioRecordRenderStage() {};

    void record(unsigned int record_position);
    void stop();
    bool is_recording() {
        return m_recording;
    }
    Tape & get_tape() {
        return m_tape;
    }
    std::weak_ptr<AudioTape> get_tape_new() {
        return m_tape_new;
    }

private:
    void render(const unsigned int time) override;

    Tape m_tape;
    std::shared_ptr<AudioTape> m_tape_new;

    bool m_recording = false;
    unsigned int m_record_position = 0;
    unsigned int m_record_start_time = 0;
};

class AudioPlaybackRenderStage : public AudioRenderStage {
public:
    AudioPlaybackRenderStage(const unsigned int frames_per_buffer,
                             const unsigned int sample_rate,
                             const unsigned int num_channels,
                             const std::string& fragment_shader_path = "build/shaders/playback_render_stage.glsl",
                             const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports);

    // Named constructor
    AudioPlaybackRenderStage(const std::string & stage_name,
                             const unsigned int frames_per_buffer,
                             const unsigned int sample_rate,
                             const unsigned int num_channels,
                             const std::string& fragment_shader_path = "build/shaders/playback_render_stage.glsl",
                             const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports)
    : AudioRenderStage(stage_name, frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {}

    static const std::vector<std::string> default_frag_shader_imports;

    ~AudioPlaybackRenderStage() {};

    void load_tape(const Tape & tape);
    void load_tape(const std::weak_ptr<AudioTape> tape);

    void play(unsigned int play_position);
    void stop();
    bool is_playing();

private:
    bool m_playing = false;
    
    void render(const unsigned int time) override;

    const unsigned int get_current_tape_position(const unsigned int time);

    void load_tape_data_to_texture(const Tape & tape, const unsigned int offset);

    const Tape * m_tape_ptr;
    std::weak_ptr<AudioTape> m_tape_ptr_new;

    std::unique_ptr<AudioRenderStageHistory2> m_history;

    static const unsigned int M_TAPE_SIZE = 200;
};

#endif // AUDIO_TAPE_RENDER_STAGE_H