#include <iostream>
#include <fstream>
#include <vector>

int main() {
    // Open the audio file
    std::ifstream audioFile("media/test.wav", std::ios::binary);

    if (!audioFile) {
        std::cerr << "Failed to open audio file." << std::endl;
        return 1;
    }

    // Get the size of the audio file
    audioFile.seekg(0, std::ios::end);
    std::streampos fileSize = audioFile.tellg();
    audioFile.seekg(0, std::ios::beg);

    // Read the audio data into a buffer
    std::vector<char> audioBuffer(fileSize);
    audioFile.read(audioBuffer.data(), fileSize);

    // Close the audio file
    audioFile.close();

    // Print the audio data
    printf("size of audioBuffer: %d\n", audioBuffer.size());

    return 0;
}