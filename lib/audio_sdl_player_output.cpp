#include <stdio.h>
#include <cstring>
#include <memory>
#include <SDL2/SDL.h>

#include "audio_sdl_player_output.h"

AudioSDLPlayerOutput::~AudioSDLPlayerOutput() {
    SDL_CloseAudioDevice(m_device_id);
    SDL_Quit();
}

bool AudioSDLOutputNew::open() {
    const char* device_name = nullptr;
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        error(SDL_GetError());
        return false;
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

bool AudioSDLPlayerOutput::open() {
    const char* device_name = nullptr;
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        error(SDL_GetError());
        return false;
    }

    SDL_AudioSpec desired_spec;
    SDL_AudioSpec obtained_spec;

    SDL_zero(desired_spec);
    desired_spec.freq = m_sample_rate;
    desired_spec.format = AUDIO_F32;
    desired_spec.channels = m_channels;
    desired_spec.samples = m_frames_per_buffer;
    desired_spec.callback = audio_callback;
    desired_spec.userdata = this;

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

bool AudioSDLPlayerOutput::start() {
    if (m_device_id == 0) {
        fprintf(stderr, "Error: Device not open.\n");
        return false;
    }
    if (m_audio_buffer_link == nullptr) {
        fprintf(stderr, "Error: Buffer not linked.\n");
        return false;
    }

    SDL_PauseAudioDevice(m_device_id, 0);
    m_is_running = true;

    printf("Started audio device.\n");
    return true;
}

bool AudioSDLOutputNew::start() {
    if (m_device_id == 0) {
        fprintf(stderr, "Error: Device not open.\n");
        return false;
    }
    m_is_running = true;
    SDL_PauseAudioDevice(m_device_id, 0);
    printf("Started audio device.\n");
    return true;
}


bool AudioSDLPlayerOutput::stop() {
    if (m_device_id == 0) {
        fprintf(stderr, "Error: Device not open.\n");
        return false;
    }

    SDL_PauseAudioDevice(m_device_id, 1);
    m_is_running = false;

    printf("Stopped audio device.\n");
    return true;
}

bool AudioSDLPlayerOutput::close() {
    if (m_device_id == 0) {
        fprintf(stderr, "Error: Device not open.\n");
        return false;
    }

    SDL_CloseAudioDevice(m_device_id);
    m_device_id = 0;

    printf("Closed audio device.\n");
    return true;
}

void AudioSDLPlayerOutput::sleep(const unsigned seconds) {
    printf("Sleeping for %d seconds.\n", seconds);
    SDL_Delay(1000 * seconds);
}

void AudioSDLPlayerOutput::audio_callback(void* userdata, Uint8* stream, int len) {
    AudioSDLPlayerOutput* audio_player_output = static_cast<AudioSDLPlayerOutput*>(userdata);

    if (!audio_player_output->m_is_running) {
        return;
    }

    auto audio_buffer = audio_player_output->m_audio_buffer_link->pop();

    std::memcpy(stream, audio_buffer, len);

    audio_player_output->update_latency();
}

void AudioSDLPlayerOutput::error(const char* message) {
    fprintf(stderr, "SDL error: %s\n", message);
    SDL_Quit();
}

void AudioSDLOutputNew::error(const char* message) {
    fprintf(stderr, "SDL error: %s\n", message);
    SDL_Quit();
}


AudioSDLOutputNew::~AudioSDLOutputNew() {
    SDL_CloseAudioDevice(m_device_id);
    SDL_Quit();
}

bool AudioSDLOutputNew::is_ready() {
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

void AudioSDLOutputNew::push(const float * data) {
    // Check if the audio device is running
    if (!m_is_running) {
        return;
    }

    int bytesToWrite = m_frames_per_buffer * m_channels * sizeof(float);
    if (SDL_QueueAudio(m_device_id, data, bytesToWrite) < 0) {
        error(SDL_GetError());
    }
}

bool AudioSDLOutputNew::stop() {
    if (m_device_id == 0) {
        fprintf(stderr, "Error: Device not open.\n");
        return false;
    }

    SDL_PauseAudioDevice(m_device_id, 1);
    m_is_running = false;

    printf("Stopped audio device.\n");
    return true;
}

bool AudioSDLOutputNew::close() {
    if (m_device_id == 0) {
        fprintf(stderr, "Error: Device not open.\n");
        return false;
    }

    SDL_CloseAudioDevice(m_device_id);
    m_device_id = 0;

    printf("Closed audio device.\n");
    return true;
}

