#include <stdio.h>
#include <cstring>
#include <memory>
#include <SDL2/SDL.h>

#include "audio_output/audio_player_output.h"

bool AudioPlayerOutput::open() {
    const char* device_name = nullptr;
    // Check if SDL is already initialized
    // If not initialized, use SDL_Init; if already initialized, use SDL_InitSubSystem
    if (SDL_WasInit(0) == 0) {
        // SDL not initialized yet, initialize it with audio subsystem
        if (SDL_Init(SDL_INIT_AUDIO) < 0) {
            error(SDL_GetError());
            return false;
        }
    } else {
        // SDL already initialized, just add the audio subsystem
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            error(SDL_GetError());
            return false;
        }
    }

    SDL_AudioSpec desired_spec;
    SDL_AudioSpec obtained_spec;
    desired_spec.freq = m_sample_rate;
    desired_spec.format = AUDIO_F32SYS;
    desired_spec.channels = m_channels;
    desired_spec.samples = m_frames_per_buffer;
    desired_spec.callback = nullptr;

    m_device_id = SDL_OpenAudioDevice(device_name, 0, &desired_spec, &obtained_spec, 0);
    if (m_device_id == 0) {
        error(SDL_GetError());
        return false;
    }

    printf("Opened audio device: %s\n", device_name ? device_name : "default");
    printf("Sample rate: %d\n", obtained_spec.freq);
    printf("Frames per buffer: %d\n", obtained_spec.samples);
    printf("Channels: %d\n", obtained_spec.channels);

    return true;
}

bool AudioPlayerOutput::start() {
    if (m_device_id == 0) {
        fprintf(stderr, "Error: Device not open.\n");
        return false;
    }
    m_is_running = true;
    SDL_PauseAudioDevice(m_device_id, 0);
    printf("Started audio device.\n");
    return true;
}

void AudioPlayerOutput::error(const char* message) {
    fprintf(stderr, "SDL error: %s\n", message);
}

AudioPlayerOutput::~AudioPlayerOutput() {
    if (m_device_id != 0) {
        clear_queue();
        SDL_CloseAudioDevice(m_device_id);
    }
    printf("AudioPlayerOutput Destroyed\n");
}

bool AudioPlayerOutput::is_ready() {
    // Check if the audio device is running
    if (!m_is_running) {
        return false;
    // Check if the queued audio size is less than the buffer size
    } else if (SDL_GetQueuedAudioSize(m_device_id) >= 2 * m_frames_per_buffer * m_channels * sizeof(float)) {
        return false;
    } else {
        return true;
    }
}

void AudioPlayerOutput::push(const float * data) {
    // Check if the audio device is running
    if (!m_is_running) {
        return;
    }

    int bytesToWrite = m_frames_per_buffer * m_channels * sizeof(float);
    if (SDL_QueueAudio(m_device_id, data, bytesToWrite) < 0) {
        error(SDL_GetError());
    }
}

bool AudioPlayerOutput::stop() {
    if (m_device_id == 0) {
        fprintf(stderr, "Error: Device not open.\n");
        return false;
    }

    SDL_PauseAudioDevice(m_device_id, 1);
    m_is_running = false;

    printf("Stopped audio device.\n");
    return true;
}

bool AudioPlayerOutput::close() {
    if (m_device_id == 0) {
        fprintf(stderr, "Error: Device not open.\n");
        return false;
    }

    // Explicitly pause just in case stop() wasn't called or failed
    SDL_PauseAudioDevice(m_device_id, 1);
    
    // Clear any queued audio to potentially unblock the consumer
    SDL_ClearQueuedAudio(m_device_id);
    
    SDL_CloseAudioDevice(m_device_id);
    m_device_id = 0;
    
    // Quit the audio subsystem to ensure clean state for next initialization
    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    printf("Closed audio device.\n");
    return true;
}

size_t AudioPlayerOutput::queued_bytes() const {
    if (m_device_id == 0) {
        return 0;
    }

    return static_cast<size_t>(SDL_GetQueuedAudioSize(m_device_id));
}

void AudioPlayerOutput::clear_queue() {
    if (m_device_id == 0) {
        return;
    }

    SDL_ClearQueuedAudio(m_device_id);
}

