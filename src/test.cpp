#include <iostream>

#include "audio_driver.h"

int main() {
    std::cout << "Hello, World!" << std::endl;
    AudioDriver audio_driver(44100, 256, 2);
    audio_driver.open();
    audio_driver.start();
    audio_driver.sleep(2);
    audio_driver.stop();
    audio_driver.close();
    return 0;
}