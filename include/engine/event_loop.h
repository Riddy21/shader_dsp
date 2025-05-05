#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <memory>
#include <thread>

// Base interface for any system that participates in the event loop.
class IEventLoopItem {
public:
    virtual ~IEventLoopItem() {}
    virtual bool is_ready() = 0;
    virtual void handle_event(const SDL_Event &event) = 0;
    virtual void render() = 0;
};

// Singleton event loop class that manages all IEventLoopItem modules.
class EventLoop {
public:
    static EventLoop& get_instance();
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    void add_loop_item(IEventLoopItem * item); // Changed interface
    void run_loop();
    void terminate();

private:
    EventLoop() = default;
    ~EventLoop();

    bool is_main_thread() const {
        return std::this_thread::get_id() == m_main_thread_id;
    }

    std::thread::id m_main_thread_id{std::this_thread::get_id()};
    std::vector<std::unique_ptr<IEventLoopItem>> m_items;
    static EventLoop* s_instance;
};
