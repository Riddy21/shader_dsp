#include <SDL2/SDL.h>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <cmath>
#include <iostream>
#include <chrono>
#include <thread>

const int SAMPLE_RATE = 48000;       // Samples per second
const int INITIAL_AMPLITUDE = 28000; // Initial amplitude of the sine wave
const int INITIAL_FREQUENCY = 440;   // Initial frequency of the sine wave (A4 note)

int amplitude = INITIAL_AMPLITUDE;
double frequency = INITIAL_FREQUENCY;
int sampleIndex = 0;

// Function to fill the audio buffer with a sine wave based on current frequency and amplitude
void fillAudioBuffer(int16_t *buffer, int bufferLength) {
    for (int i = 0; i < bufferLength; ++i) {
        buffer[i] = static_cast<int16_t>(amplitude * std::sin(2.0 * M_PI * frequency * sampleIndex / SAMPLE_RATE));
        ++sampleIndex;
    }
}

// GLUT keyboard handler to adjust frequency and amplitude
void handleKeypress(unsigned char key, int x, int y) {
    switch (key) {
        case 'w':  // Increase frequency
            frequency += 10;
            std::cout << "Frequency: " << frequency << " Hz\n";
            break;
        case 's':  // Decrease frequency
            frequency = std::max(10.0, frequency - 10); // Prevent frequency from going too low
            std::cout << "Frequency: " << frequency << " Hz\n";
            break;
        case 'd':  // Increase amplitude
            amplitude = std::min(32767, amplitude + 1000); // Cap amplitude at max 16-bit value
            std::cout << "Amplitude: " << amplitude << "\n";
            break;
        case 'a':  // Decrease amplitude
            amplitude = std::max(0, amplitude - 1000); // Prevent negative amplitude
            std::cout << "Amplitude: " << amplitude << "\n";
            break;
        case 'r':  // Reset to initial values
            frequency = INITIAL_FREQUENCY;
            amplitude = INITIAL_AMPLITUDE;
            std::cout << "Reset to Frequency: " << frequency << " Hz, Amplitude: " << amplitude << "\n";
            break;
        case 27: // ESC key to exit
            glutLeaveMainLoop();
            break;
    }
}

// Audio playback loop function to be called by GLUT idle callback
void audioPlaybackLoop() {
    // SDL audio buffer size
    const int bufferSize = 512;
    int16_t buffer[bufferSize];

    // Fill the audio buffer with sine wave data
    fillAudioBuffer(buffer, bufferSize);

    // Queue audio to SDL
    int bytesToWrite = bufferSize * sizeof(int16_t);
    if (SDL_QueueAudio(1, buffer, bytesToWrite) < 0) {
        std::cerr << "Failed to queue audio: " << SDL_GetError() << std::endl;
        glutLeaveMainLoop();
    }

    // Sleep to control the timing of playback
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 * bufferSize / SAMPLE_RATE));

    // Continue looping
    glutPostRedisplay();
}

int main(int argc, char *argv[]) {
    // Initialize SDL for audio
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Set up SDL audio specification
    SDL_AudioSpec audioSpec;
    audioSpec.freq = SAMPLE_RATE;
    audioSpec.format = AUDIO_S16SYS;
    audioSpec.channels = 1;
    audioSpec.samples = 512;
    audioSpec.callback = nullptr;

    // Open the audio device
    if (SDL_OpenAudio(&audioSpec, nullptr) < 0) {
        std::cerr << "Failed to open audio: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    // Start audio playback
    SDL_PauseAudio(0);

    // Initialize GLUT for window and keyboard input
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE);
    glutInitWindowSize(300, 300);
    glutCreateWindow("Audio Latency Test");
    glutKeyboardFunc(handleKeypress);
    glutIdleFunc(audioPlaybackLoop);

    // Start GLUT main loop
    glutMainLoop();

    // Clean up SDL
    SDL_CloseAudio();
    SDL_Quit();

    return 0;
}
