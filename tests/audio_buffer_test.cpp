#include "catch2/catch_all.hpp"
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>

#include "audio_buffer.h"

TEST_CASE("AudioQueue_push_pop") {
    // Create an audio queue
    AudioBuffer audio_queue(11);

    // Push 10 buffers
    for (int i = 0; i < 11; i++) {
        std::vector<float> buffer(i, (float)i);
        audio_queue.push(buffer);
    }

    // Pop 10 buffers
    for (int i = 0; i < 11; i++) {
        const std::vector<float> & buffer = audio_queue.pop();
        REQUIRE(buffer.size() == (unsigned)i);
    }
}

TEST_CASE("AudioQueue_push_overflow") {
    // Create an audio queue
    AudioBuffer audio_queue(10);

    // Push 10 buffers
    for (int i = 0; i < 10; i++) {
        std::vector<float> buffer(i, (float)i);
        audio_queue.push(buffer);
    }

    // Push 1 more buffer
    std::vector<float> buffer(10, 10.0);
    audio_queue.push(buffer);

    // Pop 10 buffers
    for (int i = 0; i < 10; i++) {
        const std::vector<float> & buffer = audio_queue.pop();
        if (i == 0) {
            REQUIRE(buffer.size() == 10);
        } else {
            REQUIRE(buffer.size() == (unsigned)i);
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
            audio_queue.push(buffer);
        }
    });

    // Open a thread to pop 10 buffers
    std::thread t2([&audio_queue](){
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 100));
        for (int i = 0; i < 100; i++) {
            // Sleep for a random time
            std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 10));
            const std::vector<float> & buffer = audio_queue.pop();
            REQUIRE(buffer.size() == (unsigned)i);
        }
    });

    t1.join();
    t2.join();
}