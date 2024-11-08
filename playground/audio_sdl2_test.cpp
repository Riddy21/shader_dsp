#include <SDL2/SDL.h>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <cmath>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

const int SAMPLE_RATE = 48000;       // Samples per second
const int INITIAL_AMPLITUDE = 28000; // Initial amplitude of the sine wave
const int INITIAL_FREQUENCY = 440;   // Initial frequency of the sine wave (A4 note)

std::atomic<int> amplitude(INITIAL_AMPLITUDE);
std::atomic<double> frequency(INITIAL_FREQUENCY);
std::atomic<bool> running(true);

int sampleIndex = 0;

// Function to fill the audio buffer with a sine wave based on current frequency and amplitude
void fillAudioBuffer(int16_t *buffer, int bufferLength, double freq, int amp) {
    for (int i = 0; i < bufferLength; ++i) {
        buffer[i] = static_cast<int16_t>(amp * std::sin(2.0 * M_PI * freq * sampleIndex / SAMPLE_RATE));
        ++sampleIndex;
    }
}

// Audio playback function for the separate thread
void audioPlaybackLoop() {
    const int bufferSize = 512;
    int16_t buffer[bufferSize];

    while (running) {
        // Fill the buffer with the latest frequency and amplitude values
        fillAudioBuffer(buffer, bufferSize, frequency.load(), amplitude.load());

        // Queue audio to SDL
        int bytesToWrite = bufferSize * sizeof(int16_t);
        if (SDL_QueueAudio(1, buffer, bytesToWrite) < 0) {
            std::cerr << "Failed to queue audio: " << SDL_GetError() << std::endl;
            break;
        }

        // Sleep to control the timing of playback
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 * bufferSize / SAMPLE_RATE));
    }
}

// GLUT keyboard handler to adjust frequency and amplitude
void handleKeypress(unsigned char key, int x, int y) {
    switch (key) {
        case 'w':  // Increase frequency
            frequency += 10;
            std::cout << "Frequency: " << frequency.load() << " Hz\n";
            break;
        case 's':  // Decrease frequency
            frequency = std::max(10.0, frequency.load() - 10); // Prevent frequency from going too low
            std::cout << "Frequency: " << frequency.load() << " Hz\n";
            break;
        case 'd':  // Increase amplitude
            amplitude = std::min(32767, amplitude.load() + 1000); // Cap amplitude at max 16-bit value
            std::cout << "Amplitude: " << amplitude.load() << "\n";
            break;
        case 'a':  // Decrease amplitude
            amplitude = std::max(0, amplitude.load() - 1000); // Prevent negative amplitude
            std::cout << "Amplitude: " << amplitude.load() << "\n";
            break;
        case 'r':  // Reset to initial values
            frequency = INITIAL_FREQUENCY;
            amplitude = INITIAL_AMPLITUDE;
            std::cout << "Reset to Frequency: " << frequency.load() << " Hz, Amplitude: " << amplitude.load() << "\n";
            break;
        case 27: // ESC key to exit
            running = false; // Signal the audio thread to stop
            glutLeaveMainLoop();
            break;
    }
}

// Empty display callback to satisfy GLUT
void displayCallback() {
    glClear(GL_COLOR_BUFFER_BIT);
    glutSwapBuffers();
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

    // Launch audio playback loop in a separate thread
    std::thread audioThread(audioPlaybackLoop);

    // Initialize GLUT for window and keyboard input
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(300, 300);
    glutCreateWindow("Audio Latency Test");

    // Register GLUT callbacks
    glutKeyboardFunc(handleKeypress);
    glutDisplayFunc(displayCallback); // Empty display callback to prevent errors

    // Start GLUT main loop
    glutMainLoop();

    // Clean up
    running = false;           // Signal the audio thread to stop
    audioThread.join();        // Wait for the audio thread to finish
    SDL_CloseAudio();          // Close SDL audio
    SDL_Quit();                // Quit SDL

    return 0;
}
