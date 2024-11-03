#include <SDL2/SDL.h>
#include <cmath>
#include <iostream>

class SineWaveGenerator {
public:
    SineWaveGenerator(float frequency, int sample_rate)
        : frequency(frequency), sample_rate(sample_rate), phase(0.0f) {
        phase_increment = 2.0f * M_PI * frequency / sample_rate;
    }

    void generate(float* stream, int len) {
        for (int i = 0; i < len; ++i) {
            stream[i] = sinf(phase);
            phase += phase_increment;
            if (phase >= 2.0f * M_PI) {
                phase -= 2.0f * M_PI;
            }
        }
    }

private:
    float frequency;
    int sample_rate;
    float phase;
    float phase_increment;
};

void audioCallback(void* userdata, Uint8* stream, int len) {
    SineWaveGenerator* generator = static_cast<SineWaveGenerator*>(userdata);
    float* fstream = reinterpret_cast<float*>(stream);
    int samples = len / sizeof(float);
    generator->generate(fstream, samples);
    // Write the FPS
    static int frame_count = 0;
    static double previous_time = 0.0;
    static double fps = 0.0;

    frame_count++;
    double current_time = SDL_GetTicks() / 1000.0;
    double elapsed_time = current_time - previous_time;

    if (elapsed_time > 1.0) {
        fps = frame_count / elapsed_time;
        previous_time = current_time;
        frame_count = 0;
        printf("Audio FPS: %.2f\n", fps);
    }

}

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_AudioSpec desiredSpec;
    SDL_AudioSpec obtainedSpec;
    SDL_zero(desiredSpec);

    desiredSpec.freq = 44100;
    desiredSpec.format = AUDIO_F32SYS;
    desiredSpec.channels = 2;
    desiredSpec.samples = 512;
    desiredSpec.callback = audioCallback;

    SineWaveGenerator generator(440.0f, desiredSpec.freq);
    desiredSpec.userdata = &generator;

    SDL_AudioDeviceID audioDevice = SDL_OpenAudioDevice(NULL, 0, &desiredSpec, &obtainedSpec, 0);
    if (audioDevice == 0) {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    SDL_PauseAudioDevice(audioDevice, 0);

    // Play the sine wave for 5 seconds
    SDL_Delay(5000);

    SDL_CloseAudioDevice(audioDevice);
    SDL_Quit();

    return 0;
}