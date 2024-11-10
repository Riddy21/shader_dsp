#include <SDL2/SDL.h>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <cmath>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

const int SAMPLE_RATE = 48000;       // Samples per second
const float INITIAL_AMPLITUDE = 0.5f; // Initial amplitude of the sine wave (scaled to 0-1 for float)
const float INITIAL_FREQUENCY = 440.0f; // Initial frequency of the sine wave (A4 note)
const int BUFFER_SIZE = 1024;         // Increased buffer size per channel for smoother playback

std::atomic<float> amplitude(INITIAL_AMPLITUDE);
std::atomic<float> frequency(INITIAL_FREQUENCY);
std::atomic<bool> running(true);

int sampleIndex = 0;
SDL_AudioDeviceID audioDeviceID; // Store audio device ID

// Function to fill the audio buffer with a sine wave based on current frequency and amplitude
void fillAudioBuffer(float *buffer, int bufferLength, float freq, float amp) {
    for (int i = 0; i < bufferLength; i += 2) {
        // Generate the sample for both channels (left and right)
        float sample = amp * std::sin(2.0f * M_PI * freq * sampleIndex / SAMPLE_RATE);
        buffer[i] = sample;       // Left channel
        buffer[i + 1] = sample;   // Right channel (same as left for simplicity)
        
        ++sampleIndex;
    }
}

// Audio playback function for the separate thread
void audioPlaybackLoop() {
    float buffer[BUFFER_SIZE * 2]; // Double the buffer size for stereo (2 channels)

    while (running) {
        // Check the queued audio size
        if (SDL_GetQueuedAudioSize(audioDeviceID) < 2 * BUFFER_SIZE * sizeof(float)) {
            // If there’s enough room in SDL’s audio queue, fill and queue more audio data
            fillAudioBuffer(buffer, BUFFER_SIZE * 2, frequency.load(), amplitude.load());
            int bytesToWrite = BUFFER_SIZE * 2 * sizeof(float);
            if (SDL_QueueAudio(audioDeviceID, buffer, bytesToWrite) < 0) {
                std::cerr << "Failed to queue audio: " << SDL_GetError() << std::endl;
                break;
            }
        } else {
            // Sleep briefly if the buffer is already full, to avoid busy-waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
}

// GLUT keyboard handler to adjust frequency and amplitude
void handleKeypress(unsigned char key, int x, int y) {
    switch (key) {
        case 'w':  // Increase frequency
            frequency.store(frequency.load() + 10.0f);
            std::cout << "Frequency: " << frequency.load() << " Hz\n";
            break;
        case 's':  // Decrease frequency
            frequency = std::max(10.0f, frequency.load() - 10.0f); // Prevent frequency from going too low
            std::cout << "Frequency: " << frequency.load() << " Hz\n";
            break;
        case 'd':  // Increase amplitude
            amplitude = std::min(1.0f, amplitude.load() + 0.1f); // Cap amplitude at 1.0
            std::cout << "Amplitude: " << amplitude.load() << "\n";
            break;
        case 'a':  // Decrease amplitude
            amplitude = std::max(0.0f, amplitude.load() - 0.1f); // Prevent negative amplitude
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
    audioSpec.format = AUDIO_F32SYS; // 32-bit float audio format
    audioSpec.channels = 2;          // Stereo (2 channels)
    audioSpec.samples = BUFFER_SIZE;
    audioSpec.callback = nullptr;

    // Open the audio device
    audioDeviceID = SDL_OpenAudioDevice(nullptr, 0, &audioSpec, nullptr, 0);
    if (audioDeviceID == 0) {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    // Start audio playback
    SDL_PauseAudioDevice(audioDeviceID, 0);

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
    SDL_CloseAudioDevice(audioDeviceID); // Close the specific audio device
    SDL_Quit();                // Quit SDL

    return 0;
}
