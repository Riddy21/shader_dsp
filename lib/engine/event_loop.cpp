#include "engine/event_loop.h"
#include <iostream>

// Define static instance pointer
EventLoop* EventLoop::s_instance = nullptr;

EventLoop& EventLoop::get_instance() {
    if (!s_instance) {
        s_instance = new EventLoop();
    }
    return *s_instance;
}

EventLoop::~EventLoop() {
    m_items.clear();

    SDL_Quit();
}

// Changed to accept reference and move to unique_ptr
void EventLoop::add_loop_item(IEventLoopItem * item) {
    m_items.push_back(std::unique_ptr<IEventLoopItem>(item));
}

void EventLoop::run_loop() {
    // Enforce that the event loop runs on the main thread
    if (!is_main_thread()) {
        std::cerr << "Error: EventLoop::run_loop() must be called from the main thread!" << std::endl;
        std::abort();
    }
    while (true) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // Dispatch events to each registered item
            for (auto &item : m_items) {
                if (item->is_ready()) {
                    item->handle_event(event);
                }
            }
            // If user requests exit through SDL window close
            if (event.type == SDL_QUIT) {
                printf("Received SDL_QUIT event, terminating event loop.\n");
                return;
            }
        }

        // Update and render each item
        for (auto &item : m_items) {
            if (item->is_ready()) {
                item->render();
            }
        }
    }
    SDL_Quit();
}

void EventLoop::terminate() {
    SDL_Event event;
    event.type = SDL_QUIT;
    SDL_PushEvent(&event);
}