#include <catch2/catch_all.hpp>
#include "engine/event_loop.h"
#include <memory>
#include "keyboard/keyboard.h"
#include "audio_core/audio_renderer.h"

// Test case for EventLoop with simple items
TEST_CASE("EventLoop handles keyboard and audio items", "[EventLoop]") {
    // Create an SDL window
    SDL_Window* window = SDL_CreateWindow(
        "Test Window",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    // Use a regular Keyboard instance (not singleton)
    Keyboard * keyboard = new Keyboard();

    // Get the EventLoop instance and initialize it
    EventLoop& loop = EventLoop::get_instance();

    // Add items to the event loop by reference
    loop.add_loop_item(keyboard);

    // Simulate an SDL_KEYDOWN event for the Keyboard
    Key * key = new Key('a');
    key->set_key_down_callback([]() {
        std::cout << "Key 'a' pressed!" << std::endl;
    });
    keyboard->add_key(key); // Register a key for Keyboard

    // Run in another thread the keypress simulation and then sent quit event
    std::thread keypress_thread([&loop, keyboard]() {
        // Sleep for 1 second
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Simulate an SDL_KEYDOWN event
        SDL_Event event;
        event.type = SDL_KEYDOWN;
        event.key.keysym.sym = SDLK_a;
        SDL_PushEvent(&event);

        // Sleep for a short duration and then send a quit event
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        event.type = SDL_QUIT;
        SDL_PushEvent(&event);
    });

    loop.run_loop();

    keypress_thread.join();

}