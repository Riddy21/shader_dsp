#include <iostream>
#include <unordered_map>
#include <cmath>
#include "audio_synthesizer/audio_synthesizer.h"
#include "keyboard/keyboard.h"
#include "keyboard/key.h"
#include "engine/event_loop.h"
#include "graphics/graphics_display.h"
#include "graphics_views/debug_view.h"

#define MIDDLE_C 261.63f
#define SEMI_TONE 1.059463f

const std::unordered_map<char, float> KEY_TONE_MAPPING = {
    {'a', MIDDLE_C},
    {'w', MIDDLE_C * std::pow(SEMI_TONE, 1)},
    {'s', MIDDLE_C * std::pow(SEMI_TONE, 2)},
    {'e', MIDDLE_C * std::pow(SEMI_TONE, 3)},
    {'d', MIDDLE_C * std::pow(SEMI_TONE, 4)},
    {'f', MIDDLE_C * std::pow(SEMI_TONE, 5)},
    {'t', MIDDLE_C * std::pow(SEMI_TONE, 6)},
    {'g', MIDDLE_C * std::pow(SEMI_TONE, 7)},
    {'y', MIDDLE_C * std::pow(SEMI_TONE, 8)},
    {'h', MIDDLE_C * std::pow(SEMI_TONE, 9)},
    {'u', MIDDLE_C * std::pow(SEMI_TONE, 10)},
    {'j', MIDDLE_C * std::pow(SEMI_TONE, 11)},
    {'k', MIDDLE_C * std::pow(SEMI_TONE, 12)},
};

void setup_keyboard(Keyboard& keyboard, AudioSynthesizer& synthesizer, EventLoop& event_loop) {
    for (const auto& [key, tone] : KEY_TONE_MAPPING) {
        auto key_obj = new Key(key);
        key_obj->set_key_down_callback([&synthesizer, tone]() {
            synthesizer.get_track(0).play_note(tone, 0.2f);
        });
        key_obj->set_key_up_callback([&synthesizer, tone]() {
            synthesizer.get_track(0).stop_note(tone);
        });
        keyboard.add_key(key_obj);
    }

    auto pause_key = new Key('p');
    pause_key->set_key_down_callback([&synthesizer]() {
        synthesizer.pause();
        std::cout << "Paused synthesizer." << std::endl;
    });
    keyboard.add_key(pause_key);

    auto resume_key = new Key('o');
    resume_key->set_key_down_callback([&synthesizer]() {
        synthesizer.resume();
        std::cout << "Resumed synthesizer." << std::endl;
    });
    keyboard.add_key(resume_key);

    auto increment_key = new Key('i');
    increment_key->set_key_down_callback([&synthesizer]() {
        synthesizer.increment();
        std::cout << "Incremented synthesizer." << std::endl;
    });
    keyboard.add_key(increment_key);

    auto quit_key = new Key('q');
    quit_key->set_key_down_callback([&synthesizer, &event_loop]() {
        std::cout << "Exiting program." << std::endl;
        synthesizer.close();
        event_loop.terminate();
    });
    keyboard.add_key(quit_key);

    auto record_key = new Key('r');
    record_key->set_key_down_callback([&synthesizer]() {
        //synthesizer.record();
        synthesizer.get_track(0).change_effect("echo");
        // FIXME: Rotate the list of effects next
        std::cout << "Recording..." << std::endl;
    });
    keyboard.add_key(record_key);
    auto stop_record_key = new Key('l');
    stop_record_key->set_key_down_callback([&synthesizer]() {
        //synthesizer.play_recording();
        synthesizer.get_track(0).change_voice("saw");
        // FIXME: Rotate the list of effects next
        std::cout << "Stopped recording." << std::endl;
    });
    keyboard.add_key(stop_record_key);

}

int main() {
    EventLoop& event_loop = EventLoop::get_instance();

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL2: " << SDL_GetError() << std::endl;
        return false;
    }

    // Set OpenGL version to 4.1 Core Profile for macOS compatibility
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    
    auto & synthesizer = AudioSynthesizer::get_instance();
    if (!synthesizer.initialize(512, 44100, 2)) {
        std::cerr << "Failed to initialize AudioSynthesizer." << std::endl;
        return -1;
    }

    if (!synthesizer.start()) {
        std::cerr << "Failed to start AudioSynthesizer." << std::endl;
        return -1;
    }

    Keyboard keyboard;
    setup_keyboard(keyboard, synthesizer, event_loop);

    GraphicsDisplay graphics_display = GraphicsDisplay(800, 600, "Synthesizer");
    graphics_display.register_view("debug", new DebugView());
    graphics_display.change_view("debug");

    std::cout << "Press keys to play notes. 'p' to pause, 'r' to resume, 'q' to quit." << std::endl;

    event_loop.run_loop();
    return 0;
}
