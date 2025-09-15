#include "catch2/catch_all.hpp"
#include <SDL2/SDL.h>
#include <thread>
#include <chrono>

#define private public
#undef protected

#include "graphics_core/graphics_display.h"
#include "graphics_core/graphics_view.h"
#include "engine/event_handler.h"
#include "engine/event_loop.h"

#undef private
#define protected protected

struct SDLInitGuard {
    SDLInitGuard() {
        if (SDL_WasInit(SDL_INIT_EVERYTHING) == 0) {
            if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
                throw std::runtime_error("Failed to initialise SDL");
            }
            m_we_initialised = true;
        }
    }
    ~SDLInitGuard() {
        if (m_we_initialised) {
            // SDL_Quit(); // Comment out if needed
        }
    }
private:
    bool m_we_initialised = false;
};

class MockGraphicsView : public GraphicsView {
public:
    bool initialize_called = false;
    EventHandler* passed_event_handler = nullptr;
    RenderContext passed_render_context;
    bool on_enter_called = false;
    bool on_exit_called = false;
    int render_call_count = 0;

    void initialize(EventHandler& event_handler, const RenderContext& render_context) override {
        initialize_called = true;
        passed_event_handler = &event_handler;
        passed_render_context = render_context;
        GraphicsView::initialize(event_handler, render_context);
    }

    void on_enter() override {
        on_enter_called = true;
        GraphicsView::on_enter();
    }

    void on_exit() override {
        on_exit_called = true;
        GraphicsView::on_exit();
    }

    void render() override {
        render_call_count++;
        GraphicsView::render();
    }
};

TEST_CASE("GraphicsDisplay initialization", "[graphics_display]") {
    SDLInitGuard sdl_guard;
    EventHandler& event_handler = EventHandler::get_instance();

    GraphicsDisplay display(800, 600, "Test Display", 60, event_handler);

    REQUIRE(display.get_window() != nullptr);
    REQUIRE(display.get_context() != nullptr);
    REQUIRE(display.m_width == 800);
    REQUIRE(display.m_height == 600);
    REQUIRE(display.m_title == "Test Display");
    REQUIRE(display.m_refresh_rate == 60);
    REQUIRE(&display.m_event_handler == &event_handler);
    REQUIRE(display.m_views.empty());
    REQUIRE(display.m_current_view == nullptr);
    REQUIRE(display.m_last_render_time == 0); // Assuming initialized to 0
}

TEST_CASE("GraphicsDisplay add_view", "[graphics_display]") {
    SDLInitGuard sdl_guard;
    EventHandler& event_handler = EventHandler::get_instance();

    GraphicsDisplay display(800, 600, "Test Display", 60, event_handler);

    MockGraphicsView* mock_view = new MockGraphicsView();
    display.add_view("test_view", mock_view);

    REQUIRE(display.m_views.size() == 1);
    REQUIRE(display.m_views["test_view"].get() == mock_view);
    REQUIRE(mock_view->initialize_called);
    REQUIRE(mock_view->passed_event_handler == &event_handler);
    REQUIRE(mock_view->passed_render_context.window_id == SDL_GetWindowID(display.get_window()));
    // Add more checks for render_context if needed
}

TEST_CASE("GraphicsDisplay change_view", "[graphics_display]") {
    SDLInitGuard sdl_guard;
    EventHandler& event_handler = EventHandler::get_instance();

    GraphicsDisplay display(800, 600, "Test Display", 60, event_handler);

    MockGraphicsView* view1 = new MockGraphicsView();
    MockGraphicsView* view2 = new MockGraphicsView();
    display.add_view("view1", view1);
    display.add_view("view2", view2);

    display.change_view("view1");
    REQUIRE(display.m_current_view == view1);
    REQUIRE(view1->on_enter_called);
    REQUIRE(!view1->on_exit_called);

    display.change_view("view2");
    REQUIRE(display.m_current_view == view2);
    REQUIRE(view1->on_exit_called);
    REQUIRE(view2->on_enter_called);
    REQUIRE(!view2->on_exit_called);
}

TEST_CASE("GraphicsDisplay is_ready", "[graphics_display]") {
    SDLInitGuard sdl_guard;
    EventHandler& event_handler = EventHandler::get_instance();

    GraphicsDisplay display(800, 600, "Test Display", 60, event_handler);

    // Initially, since last_render_time=0, and current_time >0, but depends on frame_duration
    Uint32 frame_duration = 1000 / 60; // ~16ms

    // Set last_render_time to now - frame_duration -1
    Uint32 now = SDL_GetTicks();
    display.m_last_render_time = now - frame_duration - 1;

    REQUIRE(display.is_ready());
    // After calling is_ready true, it sets m_last_render_time to current

    // Now it should be false immediately after
    REQUIRE(!display.is_ready());

    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(frame_duration + 1));
    REQUIRE(display.is_ready());
}

TEST_CASE("GraphicsDisplay render", "[graphics_display]") {
    SDLInitGuard sdl_guard;
    EventHandler& event_handler = EventHandler::get_instance();

    GraphicsDisplay display(800, 600, "Test Display", 60, event_handler);

    MockGraphicsView* mock_view = new MockGraphicsView();
    display.add_view("test_view", mock_view);
    display.change_view("test_view");

    // Force ready
    display.m_last_render_time = SDL_GetTicks() - (1000 / 60) - 1;

    display.render();
    REQUIRE(mock_view->render_call_count == 1);
    // Could add checks for components if added
    // Also check if base render affected FPS or something
}
