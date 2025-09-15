#include <iostream>
#include <unordered_map>
#include <cmath>
#include "audio_synthesizer/audio_synthesizer.h"
#include "engine/event_loop.h"
#include "engine/event_handler.h"
#include "graphics_core/graphics_display.h"
#include "graphics_views/debug_view.h"
#include "graphics_views/mock_interface_view.h"
#include "graphics_views/menu_view.h"

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

void setup_keyboard(AudioSynthesizer& synthesizer, EventLoop& event_loop) {
    auto & event_handler = EventHandler::get_instance();

    for (const auto& [key, tone] : KEY_TONE_MAPPING) {
        event_handler.register_entry(new KeyboardEventHandlerEntry(
            SDL_KEYDOWN, key,
            [&synthesizer, tone, key](const SDL_Event&) {
                synthesizer.get_track(0).play_note(tone, 0.2f);
                return true;
            }
        ));
        event_handler.register_entry(new KeyboardEventHandlerEntry(
            SDL_KEYUP, key,
            [&synthesizer, tone, key](const SDL_Event&) {
                synthesizer.get_track(0).stop_note(tone);
                return true;
            }
        ));
    }

    // Quit key handler
    event_handler.register_entry(new KeyboardEventHandlerEntry(
        SDL_KEYDOWN, 'q',
        [&synthesizer, &event_loop](const SDL_Event&) {
            std::cout << "Exiting program." << std::endl;
            synthesizer.close();
            event_loop.terminate();
            return true;
        }
    ));
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL2: " << SDL_GetError() << std::endl;
        return false;
    }

    EventLoop& event_loop = EventLoop::get_instance();

    auto & synthesizer = AudioSynthesizer::get_instance();
    if (!synthesizer.initialize(512, 44100, 2)) {
        std::cerr << "Failed to initialize AudioSynthesizer." << std::endl;
        return -1;
    }

    if (!synthesizer.start()) {
        std::cerr << "Failed to start AudioSynthesizer." << std::endl;
        return -1;
    }

    setup_keyboard(synthesizer, event_loop);

    GraphicsDisplay* graphics_display = new GraphicsDisplay(800, 600, "Synthesizer");
    graphics_display->add_view("debug", new DebugView());
    graphics_display->change_view("debug");

    // Create another window for the interface
    GraphicsDisplay* interface_display = new GraphicsDisplay(400, 200, "Interface");
    interface_display->add_view("debug", new DebugView());
    interface_display->add_view("interface", new MockInterfaceView());
    interface_display->add_view("menu", new MenuView());
    interface_display->change_view("menu");

   auto & event_handler = EventHandler::get_instance();

    event_handler.register_entry(new KeyboardEventHandlerEntry(
        SDL_KEYDOWN, 'z',
        [&interface_display](const SDL_Event&) {
            interface_display->change_view("interface");
            return true;
        }
    ));
    event_handler.register_entry(new KeyboardEventHandlerEntry(
        SDL_KEYDOWN, 'x',
        [&interface_display](const SDL_Event&) {
            interface_display->change_view("debug");
            return true;
        }
    ));
    event_handler.register_entry(new KeyboardEventHandlerEntry(
        SDL_KEYDOWN, 'c',
        [&interface_display](const SDL_Event&) {
            interface_display->change_view("menu");
            return true;
        }
    ));

    //Register key to toggle component outlines
    event_handler.register_entry(new KeyboardEventHandlerEntry(
        SDL_KEYDOWN, 'o',
        [](const SDL_Event&) {
            static bool show_outlines = false;
            show_outlines = !show_outlines;
            GraphicsComponent::set_global_outline(show_outlines);
            std::cout << "Component outlines " << (show_outlines ? "enabled" : "disabled") << std::endl;
            return true;
        }
    ));
    
    std::cout << "Press keys to play notes. 'p' to pause, 'r' to resume, 'q' to quit." << std::endl;
    std::cout << "Press 'o' to toggle component outlines for debugging layout." << std::endl;

    event_loop.run_loop();
    SDL_Quit();
    return 0;
}
