#include "catch2/catch_all.hpp"
#include <thread>

#include "audio_renderer.h"
#include "audio_player_output.h"
#include "keyboard.h"

TEST_CASE("KeybaordTest") {
    AudioRenderer & audio_renderer = AudioRenderer::get_instance();
    AudioPlayerOutput audio_driver(512, 44100, 2);

    Keyboard & keyboard = Keyboard::get_instance();
    PianoKey key = PianoKey('c', "media/test.wav");
    key.set_gain(1.0f);
    key.set_tone(1.0f);
    PianoKey key2 = PianoKey('d', "media/test.wav");
    key2.set_gain(1.0f);
    key2.set_tone(0.8f);
    keyboard.add_key(std::move(std::make_unique<PianoKey>(key)));
    keyboard.add_key(std::move(std::make_unique<PianoKey>(key2)));

    std::thread t1([&audio_renderer, &audio_driver, &keyboard](){
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

    REQUIRE(audio_renderer.init(512, 44100, 2));
    REQUIRE(keyboard.initialize());

    REQUIRE(audio_driver.set_buffer_link(audio_renderer.get_new_output_buffer()));
    REQUIRE(audio_driver.open());
    REQUIRE(audio_driver.start());

    audio_renderer.main_loop();

    t1.detach();

}
