#include "catch2/catch_all.hpp"
#include <thread>

#include "audio_renderer.h"
#include "audio_player_output.h"
#include "keyboard.h"

TEST_CASE("KeybaordTest") {
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    auto audio_driver = new AudioPlayerOutput(512, 44100, 2);

    Keyboard & keyboard = Keyboard::get_instance();
    auto key = new PianoKey('c', "media/test.wav");
    key->set_gain(1.0f);
    key->set_tone(1.0f);
    auto key2 = new PianoKey('d', "media/test.wav");
    key2->set_gain(1.0f);
    key2->set_tone(0.8f);

    keyboard.add_key(key);
    keyboard.add_key(key2);

    std::thread t1([&audio_renderer, &keyboard](){
        Key * key = keyboard.get_key('c');
        Key * key2 = keyboard.get_key('d');
        std::this_thread::sleep_for(std::chrono::seconds(1));
        key->key_down();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        key2->key_down();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        key->key_up();
        key2->key_up();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        audio_renderer.terminate();
    });

    audio_renderer.add_render_output(audio_driver);

    REQUIRE(audio_renderer.initialize(512, 44100, 2));
    REQUIRE(keyboard.initialize());

    REQUIRE(audio_driver->open());
    REQUIRE(audio_driver->start());

    audio_renderer.start_main_loop();

    t1.detach();

}
