#include <iostream>

#include "engine/event_loop.h"
#include "engine/renderable_item.h"
#include "engine/event_handler.h"

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
void EventLoop::add_loop_item(IRenderableEntity * item) {
    m_items.push_back(std::unique_ptr<IRenderableEntity>(item));
}

void EventLoop::add_event_handler(EventHandler* handler) {
    m_event_handlers.push_back(handler);
}

void EventLoop::run_loop() {
    // Enforce that the event loop runs on the main thread
    if (!is_main_thread()) {
        std::cerr << "Error: EventLoop::run_loop() must be called from the main thread!" << std::endl;
        std::abort();
    }

    Uint32 last_time = SDL_GetTicks();
    Uint32 frame_count = 0;

    // Render everything to start
    for (auto &item : m_items) {
        item->activate_render_context();
        item->render();
    }

    while (true) {
        SDL_Event event;
        bool event_occurred = false;
        while (SDL_PollEvent(&event)) {
            // Dispatch SDL events to registered event handlers only
            for (auto* handler : m_event_handlers) {
                if (handler->handle_event(event)) {
                    event_occurred = true;
                }
            }
            // If user requests exit through SDL window close
            if (event.type == SDL_QUIT) {
                printf("Received SDL_QUIT event, terminating event loop.\n");
                return;
            }
        }

        // Render again if an event occurred
        if (event_occurred) {
            for (auto &item : m_items) {
                item->activate_render_context();
                item->render();
            }
        }

        // Update and render each item
        for (auto &item : m_items) {
            if (item->is_ready()) { // TODO: Instead of rendering when ready, render on interval, and push only when ready
                item->activate_render_context();
                item->present(); // Call present to update the display
                item->render();
            }
        }

        // Increment frame count
        frame_count++;

        // Calculate and print FPS every second
        //Uint32 current_time = SDL_GetTicks();
        //if (current_time - last_time >= 1000) {
        //    float fps = frame_count / ((current_time - last_time) / 1000.0f);
        //    std::cout << "FPS: " << fps << std::endl;

        //    // Print individual FPS for each item
        //    for (const auto &item : m_items) {
        //        std::cout << "Render FPS: " << item->get_render_fps()
        //                  << ", Present FPS: " << item->get_present_fps() << std::endl;
        //    }

        //    frame_count = 0;
        //    last_time = current_time;
        //}
    }

    SDL_Quit();
}

void EventLoop::terminate() {
    SDL_Event event;
    event.type = SDL_QUIT;
    SDL_PushEvent(&event);
}

