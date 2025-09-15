#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <memory>
#include <thread>

// Forward declaration
class EventHandler;
class IRenderableEntity;

// Singleton event loop class that manages all IEventLoopItem modules.
class EventLoop {
public:
    static EventLoop& get_instance();
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    void add_loop_item(IRenderableEntity * item); // Changed interface
    void add_event_handler(EventHandler* handler); // New method
    void run_loop();
    void terminate();
    void remove_loop_item(IRenderableEntity* item);

private:
    EventLoop() = default;
    ~EventLoop();

    bool is_main_thread() const {
        return std::this_thread::get_id() == m_main_thread_id;
    }

    std::thread::id m_main_thread_id{std::this_thread::get_id()};
    std::vector<std::unique_ptr<IRenderableEntity>> m_items;
    std::vector<EventHandler*> m_event_handlers; // New member
    static EventLoop* s_instance;
};
