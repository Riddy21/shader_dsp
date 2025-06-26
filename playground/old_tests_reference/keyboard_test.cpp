#include "catch2/catch_all.hpp"
#include <thread>

#include "audio_core/audio_renderer.h"
#include "audio_output/audio_player_output.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_core/audio_render_graph.h"
#include "keyboard/keyboard.h"
#include "engine/event_loop.h"

TEST_CASE("KeybaordTest") {
    AudioRenderer& audio_renderer = AudioRenderer::get_instance();
    EventLoop& event_loop = EventLoop::get_instance();

    Keyboard * keyboard = new Keyboard();

    event_loop.add_loop_item(keyboard);


    auto new_key = new Key('a');
    new_key->set_key_down_callback([]() {
        std::cout << "Key 'a' pressed!" << std::endl;
    });

    keyboard->add_key(new_key);

    // Simulate an SDL_KEYDOWN event for the Keyboard
    SDL_Event event;
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_a;
    SDL_PushEvent(&event);

    // Run the event loop in a separate thread
    std::thread event_loop_thread([&event_loop]() {
        event_loop.run_loop();
    });
}
