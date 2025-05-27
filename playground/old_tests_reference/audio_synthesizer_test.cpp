#include <catch2/catch_all.hpp>
#include <thread>

#include "audio_synthesizer/audio_synthesizer.h"
#include "engine/event_loop.h"

TEST_CASE("AudioSynthesizer Initialization", "[AudioSynthesizer]") {
    AudioSynthesizer& synthesizer = AudioSynthesizer::get_instance();
    EventLoop& event_loop = EventLoop::get_instance();

    // Initialize the synthesizer with default parameters
    REQUIRE(synthesizer.initialize(512, 44100, 2));

    synthesizer.start();

    std::thread test_thread([&]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Play a note
        printf("Playing note...\n");
        synthesizer.play_note(MIDDLE_C, 0.5f);

        std::this_thread::sleep_for(std::chrono::seconds(1));

        REQUIRE(synthesizer.pause());

        std::this_thread::sleep_for(std::chrono::seconds(1));

        REQUIRE(synthesizer.increment());

        std::this_thread::sleep_for(std::chrono::seconds(1));

        REQUIRE(synthesizer.increment());

        std::this_thread::sleep_for(std::chrono::seconds(1));

        REQUIRE(synthesizer.increment());

        std::this_thread::sleep_for(std::chrono::seconds(1));

        REQUIRE(synthesizer.resume());

        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Stop the note
        synthesizer.stop_note(MIDDLE_C);
        printf("Stopped note...\n");

        std::this_thread::sleep_for(std::chrono::seconds(1));

        event_loop.terminate();
    });

    event_loop.run_loop();

    test_thread.join();
}