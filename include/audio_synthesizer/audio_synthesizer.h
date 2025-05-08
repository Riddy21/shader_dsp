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
#include "audio_render_stage/audio_generator_render_stage.h"
#include "audio_render_stage/audio_tape_render_stage.h"


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

    // Functions to start and stop the synthesizer
    bool start();
    bool pause();
    bool resume();
    bool increment();
    bool close();

    // Consider moving this function to engine object
    void play_note(const float tone, const float volume);
    void stop_note(const float tone);

    // TODO: Add functions for parameter nobs and buttons

    // TODO: Add functions to switch out the Outputs

    // TODO: Add function to switch out effects

    // TODO: Add track architeture

    // TODO: Add recording feature
    void record(); // TODO: add which track to record to  Add wheree to start recording
    void stop_recording(); // TODO: add which track to stop recording to
    void play_recording(); // TODO: add which track to play recording from

    // TODO: Should be able to get the sound after any render stage

    const std::vector<std::vector<float>> & get_audio_data() {
        return m_render_graph->get_output_render_stage()->get_output_data_channel_seperated();
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
    std::unordered_map<std::string, AudioRenderStage *> m_render_stages;

    // FIXME: Delete this after testing
    AudioGeneratorRenderStage * m_audio_generator = nullptr;
    AudioRecordRenderStage * m_audio_recorder = nullptr;
    AudioPlaybackRenderStage * m_audio_playback = nullptr;

    // TODO: Should be able to take in Output objects
    // TODO: Should be able to take in effect objects
    // TODO: Should be able to take in engine objects

    static AudioSynthesizer* instance;
};

#endif // AUDIO_SYNTHESIZER_H