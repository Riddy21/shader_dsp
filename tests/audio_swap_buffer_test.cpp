#include <catch2/catch_all.hpp>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <iostream>

#include "audio_buffers/audio_swap_buffer.h"

std::atomic<int> count(0);
std::mutex mtx;

void writer_thread(AudioSwapBuffer& buffer, const unsigned int buffer_size, std::atomic<bool>& running) {
    float* data = new float[buffer_size];
    int writer_frames = 0;
    auto start_time = std::chrono::steady_clock::now();

    while (running.load()) {
        for (unsigned int j = 0; j < buffer_size; ++j) {
            data[j] = static_cast<float>(j + count.load());
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }

        buffer.write_buffer(data);

        writer_frames++;

        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
        if (elapsed_time >= 1) {
            std::cout << "Writer FPS: " << writer_frames / elapsed_time << std::endl;
            writer_frames = 0;
            start_time = current_time;
        }
    }

    delete[] data;
}

void reader_thread(AudioSwapBuffer& buffer, const unsigned int buffer_size, std::atomic<bool>& running) {
    int reader_frames = 0;
    auto start_time = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Wait for the writer to start

    while (running.load()) {
        count++;
        buffer.swap_buffers();
        const float* read_data = buffer.read_buffer();
        // Check if the buffer is being read correctly
        for (unsigned int j = 0; j < buffer_size; ++j) {
            REQUIRE(read_data[j] == static_cast<float>(j + count.load() - 1));
        }
        reader_frames++;

        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
        if (elapsed_time >= 1) {
            std::cout << "Reader FPS: " << reader_frames / elapsed_time << std::endl;
            reader_frames = 0;
            start_time = current_time;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(12)); // 85 fps
    }
}

TEST_CASE("AudioSwapBuffer MultiThreaded ReadWrite", "[AudioSwapBuffer]") {
    const unsigned int buffer_size = 10;
    const unsigned int duration_seconds = 10;
    AudioSwapBuffer buffer(buffer_size);

    std::atomic<bool> running(true);

    std::thread writer(writer_thread, std::ref(buffer), buffer_size, std::ref(running));
    std::thread reader(reader_thread, std::ref(buffer), buffer_size, std::ref(running));

    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
    running.store(false);

    writer.join();
    reader.join();
}