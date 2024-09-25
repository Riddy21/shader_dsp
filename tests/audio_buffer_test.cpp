#include "catch2/catch_all.hpp"
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>

#include "audio_buffer.h"

// FIXME: Fix these unit tests
TEST_CASE("AudioQueue_push_pop") {
    // Create an audio queue
    AudioBuffer audio_queue(11);

    // Push 10 buffers
    for (int i = 0; i < 10; i++) {
        std::vector<float> buffer(i, (float)i);
        audio_queue.push(buffer.data(), buffer.size());
    }

    // Pop 10 buffers
    for (int i = 0; i < 10; i++) {
        const float * buffer = audio_queue.pop();
        if (i != 0) {
            REQUIRE((unsigned)buffer[i-1] == (unsigned)i);
        }
    }
}

TEST_CASE("AudioQueue_push_underflow") {
    // Create an audio queue
    AudioBuffer audio_queue(10);

    // Push 5 buffers
    for (int i = 0; i < 15; i++) {
        std::vector<float> buffer(i, (float)i);
        audio_queue.push(buffer.data(), buffer.size());
    }

    // Pop 10 buffers
    for (int i = 0; i < 10; i++) {
        const float * buffer = audio_queue.pop();
        if (i == 0) {
        }
        else if (i < 5) {
            REQUIRE((unsigned)buffer[i-1] == (unsigned)i+10);
        } else {
        }
    }
}

TEST_CASE("AudioQueue_push_pop_threaded") {

    // Create an audio queue
    AudioBuffer audio_queue(40);

    // Open a thread to push 10 buffers
    std::thread t1([&audio_queue](){
        for (int i = 0; i < 100; i++) {
            // Sleep for a random time
            std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 10));
            std::vector<float> buffer(i, (float)i);
            audio_queue.push(buffer.data(), buffer.size());
        }
    });

    // Open a thread to pop 10 buffers
    std::thread t2([&audio_queue](){
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 100));
        for (int i = 0; i < 100; i++) {
            // Sleep for a random time
            std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 10));
            const float * buffer = audio_queue.pop();
            REQUIRE((unsigned)buffer[i-1] == (unsigned)i);
        }
    });

    t1.join();
    t2.join();
}