#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <memory>
#include <thread>

// Base interface for any system that participates in the event loop.
class IEventLoopItem {
public:
    virtual ~IEventLoopItem() {}
    virtual bool initialize() = 0;
    virtual bool is_ready() const = 0;
    virtual void handle_event(const SDL_Event &event) = 0;
    virtual void render() = 0;
};

// Singleton event loop class that manages all IEventLoopItem modules.
class EventLoop {
public:
    static EventLoop& get_instance();
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    bool initialize();
    void add_loop_item(IEventLoopItem * item); // Changed interface
    void run_loop();
    void terminate();

private:
    EventLoop() = default;
    ~EventLoop() = default;

    bool is_main_thread() const {
        return std::this_thread::get_id() == m_main_thread_id;
    }

    std::thread::id m_main_thread_id{std::this_thread::get_id()};
    bool m_running{false};
    std::vector<std::unique_ptr<IEventLoopItem>> m_items;
    static EventLoop* s_instance;
};
