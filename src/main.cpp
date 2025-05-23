#include <iostream>
#include <unordered_map>
#include <cmath>
#include "audio_synthesizer/audio_synthesizer.h"
#include "engine/event_loop.h"
#include "engine/event_handler.h"
#include "graphics/graphics_display.h"
#include "graphics_views/debug_view.h"
#include "graphics_views/mock_interface_view.h"

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

void setup_keyboard(EventHandler& event_handler, AudioSynthesizer& synthesizer, EventLoop& event_loop) {
    for (const auto& [key, tone] : KEY_TONE_MAPPING) {
        event_handler.register_entry(new KeyboardEventHandlerEntry(
            &AudioRenderer::get_instance(),
            nullptr,
            SDL_KEYDOWN, key,
            [&synthesizer, tone, key](const SDL_Event&) {
                synthesizer.get_track(0).play_note(tone, 0.2f);
                return true;
            }
        ));
        event_handler.register_entry(new KeyboardEventHandlerEntry(
            &AudioRenderer::get_instance(),
            nullptr,
            SDL_KEYUP, key,
            [&synthesizer, tone, key](const SDL_Event&) {
                synthesizer.get_track(0).stop_note(tone);
                return true;
            }
        ));
    }

    event_handler.register_entry(new KeyboardEventHandlerEntry(
        &AudioRenderer::get_instance(),
        nullptr,
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
    EventLoop& event_loop = EventLoop::get_instance();

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL2: " << SDL_GetError() << std::endl;
        return false;
    }

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

    EventHandler* event_handler = new EventHandler();
    setup_keyboard(*event_handler, synthesizer, event_loop);

    GraphicsDisplay* graphics_display = new GraphicsDisplay(800, 600, "Synthesizer");
    graphics_display->set_event_handler(event_handler);
    graphics_display->register_view("debug", new DebugView(graphics_display, event_handler));
    graphics_display->change_view("debug");

    // Create another window for the interface
    GraphicsDisplay* interface_display = new GraphicsDisplay(400, 200, "Interface");
    interface_display->set_event_handler(event_handler);
    interface_display->register_view("debug", new DebugView(interface_display, event_handler));
    interface_display->register_view("interface", new MockInterfaceView(interface_display, event_handler));
    interface_display->change_view("interface");

    std::cout << "Press keys to play notes. 'p' to pause, 'r' to resume, 'q' to quit." << std::endl;

    event_loop.run_loop();
    return 0;
}
