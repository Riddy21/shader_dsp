#pragma once
#ifndef AUDIO_SYNTHESIZER_H
#define AUDIO_SYNTHESIZER_H

#include <vector>
#include <memory>
#include <iostream>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>

#include "audio_core/audio_renderer.h"
#include "audio_core/audio_render_graph.h"
#include "audio_core/audio_render_stage.h"
#include "audio_output/audio_output.h"
#include "audio_core/audio_parameter.h"
// FIXME: Delete this after testing

#include "audio_synthesizer/audio_track.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_render_stage/audio_multitrack_join_render_stage.h"

class AudioSynthesizer {
public:
    static AudioSynthesizer& get_instance() {
        if (!instance) {
            instance = new AudioSynthesizer();
        }
        return *instance;
    }

    AudioSynthesizer(AudioSynthesizer const&) = delete;
    void operator=(AudioSynthesizer const&) = delete;

    // Function to initialize the synthesizer
    bool initialize(const unsigned int buffer_size = 512,
                    const unsigned int sample_rate = 44100,
                    const unsigned int num_channels = 2);
    bool terminate();

    // Track manipulation
    bool add_track(AudioTrack * track);
    bool remove_track(AudioTrack * track);
    AudioTrack & get_track(const unsigned int track_id) {
        if (track_id >= m_tracks.size()) {
            std::cerr << "Track ID out of range." << std::endl;
            return *m_tracks[0].get();
        }
        return *m_tracks[track_id].get();
    }

    // Functions to start and stop the synthesizer
    bool start();
    bool pause();
    bool resume();
    bool increment();
    bool close();

    // TODO: Add API for saving and recording to file
    void save_to_file(const std::string & file_path);

    // Getting audio data output
    const std::vector<std::vector<float>> & get_audio_data() {
        return m_final_render_stage->get_output_data_channel_seperated();
    }

private:
    AudioSynthesizer();
    ~AudioSynthesizer();

    unsigned int m_buffer_size = 512; // Size of audio data
    unsigned int m_sample_rate = 44100; // Sample rate of audio data
    unsigned int m_num_channels = 2; // Number of audio channels

    AudioRenderer & m_audio_renderer;
    AudioRenderGraph * m_render_graph;
    std::vector<AudioOutput *> m_render_outputs;
    AudioFinalRenderStage * m_final_render_stage;
    AudioMultitrackJoinRenderStage * m_audio_join;
    std::vector<std::unique_ptr<AudioTrack>> m_tracks;

    static AudioSynthesizer* instance;
};

#endif // AUDIO_SYNTHESIZER_H