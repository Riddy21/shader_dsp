#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

class SDL2AudioTest {
private:
    Mix_Chunk* testSound;
    bool running;

public:
    SDL2AudioTest() : testSound(nullptr), running(false) {}

    ~SDL2AudioTest() {
        cleanup();
    }

    bool init() {
        // Initialize SDL
        if (SDL_Init(SDL_INIT_AUDIO) < 0) {
            std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }

        // Initialize SDL_mixer
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
            std::cerr << "SDL_mixer could not initialize! Mix_Error: " << Mix_GetError() << std::endl;
            return false;
        }

        // Generate test sounds
        generateTestSounds();

        std::cout << "SDL2 Audio Test initialized successfully!" << std::endl;
        std::cout << "SDL2_mixer version: " << Mix_Linked_Version()->major << "." 
                  << Mix_Linked_Version()->minor << "." << Mix_Linked_Version()->patch << std::endl;
        
        int frequency, channels;
        Uint16 format;
        if (Mix_QuerySpec(&frequency, &format, &channels)) {
            std::cout << "Audio format: " << frequency << " Hz, " << channels << " channels" << std::endl;
        }
        std::cout << std::endl;

        return true;
    }

    void generateTestSounds() {
        // Parameters
        const int sampleRate = 44100;
        const int duration = 2; // seconds
        const int channels = 2;
        const int bitsPerSample = 16;
        const int bytesPerSample = bitsPerSample / 8;
        const int numFrames = sampleRate * duration;
        const int numSamples = numFrames * channels;
        const int dataSize = numSamples * bytesPerSample;

        // WAV header
        unsigned char wavHeader[44];
        int fileSize = 44 + dataSize - 8;
        int byteRate = sampleRate * channels * bytesPerSample;
        int blockAlign = channels * bytesPerSample;

        // RIFF header
        memcpy(wavHeader, "RIFF", 4);
        *(int*)(wavHeader + 4) = fileSize;
        memcpy(wavHeader + 8, "WAVE", 4);

        // fmt subchunk
        memcpy(wavHeader + 12, "fmt ", 4);
        *(int*)(wavHeader + 16) = 16; // Subchunk1Size
        *(short*)(wavHeader + 20) = 1; // AudioFormat PCM
        *(short*)(wavHeader + 22) = channels;
        *(int*)(wavHeader + 24) = sampleRate;
        *(int*)(wavHeader + 28) = byteRate;
        *(short*)(wavHeader + 32) = blockAlign;
        *(short*)(wavHeader + 34) = bitsPerSample;

        // data subchunk
        memcpy(wavHeader + 36, "data", 4);
        *(int*)(wavHeader + 40) = dataSize;

        // Generate sine wave data
        std::vector<unsigned char> wavData(44 + dataSize);
        memcpy(wavData.data(), wavHeader, 44);

        double freq = 440.0;
        for (int i = 0; i < numFrames; ++i) {
            double t = (double)i / sampleRate;
            short sample = (short)(32767.0 * sin(2.0 * M_PI * freq * t));
            // Stereo: write same sample to both channels
            wavData[44 + i * 4 + 0] = sample & 0xFF;
            wavData[44 + i * 4 + 1] = (sample >> 8) & 0xFF;
            wavData[44 + i * 4 + 2] = sample & 0xFF;
            wavData[44 + i * 4 + 3] = (sample >> 8) & 0xFF;
        }

        SDL_RWops* rw = SDL_RWFromMem(wavData.data(), wavData.size());
        testSound = Mix_LoadWAV_RW(rw, 0);

        if (!testSound) {
            std::cerr << "Failed to generate test sound! Mix_Error: " << Mix_GetError() << std::endl;
        } else {
            std::cout << "✓ Test sound generated (440Hz sine wave)" << std::endl;
        }
    }

    void run() {
        running = true;
        
        std::cout << "=== SDL2 Audio Test Menu ===" << std::endl;
        std::cout << "1. Play test sound (440Hz sine wave)" << std::endl;
        std::cout << "2. Play test sound multiple times" << std::endl;
        std::cout << "3. Continuous playback test" << std::endl;
        std::cout << "4. Volume control test" << std::endl;
        std::cout << "5. Exit" << std::endl;
        std::cout << std::endl;
        
        while (running) {
            std::cout << "Enter choice (1-5): ";
            int choice;
            std::cin >> choice;
            
            switch (choice) {
                case 1:
                    playSingleTest();
                    break;
                case 2:
                    playMultipleTests();
                    break;
                case 3:
                    continuousPlaybackTest();
                    break;
                case 4:
                    volumeControlTest();
                    break;
                case 5:
                    running = false;
                    break;
                default:
                    std::cout << "Invalid choice. Please enter 1-5." << std::endl;
                    break;
            }
        }
    }

    void playSingleTest() {
        if (testSound) {
            std::cout << "Playing test sound..." << std::endl;
            Mix_PlayChannel(-1, testSound, 0);
            
            // Wait for sound to finish
            while (Mix_Playing(-1)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            std::cout << "Test sound completed." << std::endl;
        } else {
            std::cout << "No test sound available!" << std::endl;
        }
    }

    void playMultipleTests() {
        if (testSound) {
            std::cout << "Playing test sound 3 times..." << std::endl;
            for (int i = 0; i < 3; i++) {
                std::cout << "Playing sound " << (i + 1) << "/3..." << std::endl;
                Mix_PlayChannel(-1, testSound, 0);
                
                // Wait for sound to finish
                while (Mix_Playing(-1)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                
                if (i < 2) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
            }
            std::cout << "Multiple test completed." << std::endl;
        } else {
            std::cout << "No test sound available!" << std::endl;
        }
    }

    void continuousPlaybackTest() {
        if (testSound) {
            std::cout << "Starting continuous playback test..." << std::endl;
            std::cout << "Press Enter to stop..." << std::endl;
            
            // Start continuous playback
            Mix_PlayChannel(0, testSound, -1); // Loop indefinitely
            
            // Wait for user input
            std::cin.ignore();
            std::cin.get();
            
            // Stop playback
            Mix_HaltChannel(0);
            std::cout << "Continuous playback stopped." << std::endl;
        } else {
            std::cout << "No test sound available!" << std::endl;
        }
    }

    void volumeControlTest() {
        if (testSound) {
            std::cout << "Volume control test..." << std::endl;
            std::cout << "Playing at different volumes..." << std::endl;
            
            // Test different volumes
            for (int volume = 128; volume >= 32; volume -= 32) {
                std::cout << "Volume: " << (volume * 100 / 128) << "%" << std::endl;
                Mix_Volume(-1, volume);
                Mix_PlayChannel(-1, testSound, 0);
                
                // Wait for sound to finish
                while (Mix_Playing(-1)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            }
            
            // Reset volume
            Mix_Volume(-1, MIX_MAX_VOLUME);
            std::cout << "Volume control test completed." << std::endl;
        } else {
            std::cout << "No test sound available!" << std::endl;
        }
    }

    void cleanup() {
        if (testSound) {
            Mix_FreeChunk(testSound);
            testSound = nullptr;
        }
        
        Mix_Quit();
        SDL_Quit();
    }
};

int main(int argc, char* argv[]) {
    std::cout << "=== SDL2 Audio Test Program ===" << std::endl;
    std::cout << "Testing SDL2 audio with PulseAudio integration" << std::endl;
    std::cout << std::endl;
    
    // Check PulseAudio connection
    std::cout << "Checking PulseAudio connection..." << std::endl;
    FILE* pulseCheck = popen("pactl info 2>/dev/null", "r");
    if (pulseCheck) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pulseCheck)) {
            std::cout << "✓ PulseAudio connection successful" << std::endl;
        } else {
            std::cout << "⚠ Warning: PulseAudio connection may not be working" << std::endl;
        }
        pclose(pulseCheck);
    }
    
    std::cout << std::endl;
    
    SDL2AudioTest test;
    
    if (!test.init()) {
        std::cerr << "Failed to initialize SDL2 audio test!" << std::endl;
        return 1;
    }
    
    test.run();
    
    std::cout << "SDL2 audio test completed successfully!" << std::endl;
    return 0;
} 