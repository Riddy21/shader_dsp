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
    virtual void handle_event(const SDL_Event &event) {};
    virtual void render() {};
    virtual void present() {}; // New function to model when the output gets put on the display

    // Function to calculate FPS for rendering
    virtual float get_render_fps() const { return m_render_fps; }

    // Function to calculate FPS for presentation
    virtual float get_present_fps() const { return m_present_fps; }

protected:
    void update_render_fps() {
        auto now = SDL_GetTicks();
        if (m_last_render_time > 0) {
            m_render_frame_count++;
            if (now - m_last_render_time >= 1000) {
                m_render_fps = m_render_frame_count * 1000.0f / (now - m_last_render_time);
                m_render_frame_count = 0;
                m_last_render_time = now;
            }
        } else {
            m_last_render_time = now;
        }
    }

    void update_present_fps() {
        auto now = SDL_GetTicks();
        if (m_last_present_time > 0) {
            m_present_frame_count++;
            if (now - m_last_present_time >= 1000) {
                m_present_fps = m_present_frame_count * 1000.0f / (now - m_last_present_time);
                m_present_frame_count = 0;
                m_last_present_time = now;
            }
        } else {
            m_last_present_time = now;
        }
    }

private:
    float m_render_fps{0.0f};
    float m_present_fps{0.0f};
    Uint32 m_last_render_time{0};
    Uint32 m_last_present_time{0};
    int m_render_frame_count{0};
    int m_present_frame_count{0};
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
