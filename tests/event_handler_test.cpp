#ifndef EVENT_HANDLER_TEST_CPP
#define EVENT_HANDLER_TEST_CPP

#include "catch2/catch_all.hpp"

#include <SDL2/SDL.h>
#include <functional>
#include <memory>

#include "engine/event_handler.h"
#include "engine/renderable_entity.h"

// Small RAII helper to ensure SDL is initialised for each test case
struct SDLInitGuard {
    SDLInitGuard()  {
        if(SDL_WasInit(SDL_INIT_VIDEO) == 0) {
            if(SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
                throw std::runtime_error("Failed to initialise SDL video subsystem");
            }
            m_we_initialised = true;
        }
    }
    ~SDLInitGuard() {
        if(m_we_initialised) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }
    }
private:
    bool m_we_initialised = false;
};

// Dummy implementation of IRenderableEntity used only for testing to get a valid RenderContext
class DummyRenderableEntity : public IRenderableEntity {
public:
    DummyRenderableEntity(unsigned int w = 800, unsigned int h = 600, bool visible = false)
    {
        initialize_sdl(w, h, "Dummy", visible ? SDL_WINDOW_SHOWN : SDL_WINDOW_HIDDEN, visible);
    }

    bool is_ready() override { return true; }

    void render() override {
        // Noop
    }

    void present() override {
        // Noop
    }
};

TEST_CASE("EventHandler singleton", "[event_handler]") {
    auto& eh1 = EventHandler::get_instance();
    auto& eh2 = EventHandler::get_instance();
    REQUIRE(&eh1 == &eh2);
}

TEST_CASE("EventHandler register and unregister entry", "[event_handler]") {
    SDLInitGuard sdl_guard;
    DummyRenderableEntity dummy;
    RenderContext ctx = dummy.get_render_context();

    auto entry = std::make_shared<KeyboardEventHandlerEntry>(SDL_KEYDOWN, SDLK_a, [](const SDL_Event&){ return true; }, false, ctx);

    EventHandler& eh = EventHandler::get_instance();
    eh.register_entry(entry);

    // To check if registered, we can handle an event and see, but for now, assume it works if no crash

    auto removed = eh.unregister_entry(entry);
    REQUIRE(removed != nullptr);
}

TEST_CASE("EventHandler handle event no match", "[event_handler]") {
    SDLInitGuard sdl_guard;
    DummyRenderableEntity dummy;
    RenderContext ctx = dummy.get_render_context();

    EventHandler& eh = EventHandler::get_instance();

    SDL_Event ev;
    ev.type = SDL_KEYDOWN;
    ev.key.keysym.sym = SDLK_a;

    bool handled = eh.handle_event(ev);
    REQUIRE_FALSE(handled);
}

TEST_CASE("KeyboardEventHandlerEntry matching and callback", "[event_handler][keyboard]") {
    SDLInitGuard sdl_guard;
    DummyRenderableEntity dummy;
    RenderContext ctx = dummy.get_render_context();

    bool called = false;
    auto cb = [&called](const SDL_Event& e) {
        called = true;
        return true;
    };

    auto entry = std::make_shared<KeyboardEventHandlerEntry>(SDL_KEYDOWN, SDLK_a, cb, false, ctx);
    EventHandler& eh = EventHandler::get_instance();
    eh.register_entry(entry);

    SDL_Event ev;
    ev.type = SDL_KEYDOWN;
    ev.key.windowID = ctx.window_id;
    ev.key.keysym.sym = SDLK_a;

    bool handled = eh.handle_event(ev);
    REQUIRE(handled);
    REQUIRE(called);

    // Cleanup
    eh.unregister_entry(entry);
}

TEST_CASE("KeyboardEventHandlerEntry sticky keys", "[event_handler][keyboard]") {
    SDLInitGuard sdl_guard;
    DummyRenderableEntity dummy;
    RenderContext ctx = dummy.get_render_context();

    int call_count = 0;
    auto cb = [&call_count](const SDL_Event&) { call_count++; return true; };

    auto entry = std::make_shared<KeyboardEventHandlerEntry>(SDL_KEYDOWN, SDLK_a, cb, false, ctx); // non-sticky
    EventHandler& eh = EventHandler::get_instance();
    eh.register_entry(entry);

    SDL_Event down;
    down.type = SDL_KEYDOWN;
    down.key.windowID = ctx.window_id;
    down.key.keysym.sym = SDLK_a;

    eh.handle_event(down); // first down
    REQUIRE(call_count == 1);

    eh.handle_event(down); // repeat down, should not call if non-sticky
    REQUIRE(call_count == 1);

    // Now sticky
    eh.unregister_entry(entry);
    entry = std::make_shared<KeyboardEventHandlerEntry>(SDL_KEYDOWN, SDLK_a, cb, true, ctx);
    eh.register_entry(entry);

    call_count = 0;
    eh.handle_event(down);
    REQUIRE(call_count == 1);

    eh.handle_event(down); // should call again if sticky
    REQUIRE(call_count == 2);

    // Cleanup
    eh.unregister_entry(entry);
}

TEST_CASE("KeyboardEventHandlerEntry wrong window", "[event_handler][keyboard]") {
    SDLInitGuard sdl_guard;
    DummyRenderableEntity dummy1;
    RenderContext ctx1 = dummy1.get_render_context();
    DummyRenderableEntity dummy2;
    RenderContext ctx2 = dummy2.get_render_context();

    bool called = false;
    auto cb = [&called](const SDL_Event&) { called = true; return true; };

    auto entry = std::make_shared<KeyboardEventHandlerEntry>(SDL_KEYDOWN, SDLK_a, cb, false, ctx1);
    EventHandler& eh = EventHandler::get_instance();
    eh.register_entry(entry);

    SDL_Event ev;
    ev.type = SDL_KEYDOWN;
    ev.key.windowID = ctx2.window_id; // wrong window
    ev.key.keysym.sym = SDLK_a;

    bool handled = eh.handle_event(ev);
    REQUIRE_FALSE(handled);
    REQUIRE_FALSE(called);

    // Cleanup
    eh.unregister_entry(entry);
}

// Similar tests for MouseClickEventHandlerEntry

TEST_CASE("MouseClickEventHandlerEntry matching", "[event_handler][mouse]") {
    SDLInitGuard sdl_guard;
    DummyRenderableEntity dummy(800, 600, false);
    RenderContext ctx = dummy.get_render_context();

    bool called = false;
    auto cb = [&called](const SDL_Event&) { called = true; return true; };

    // Region: normalized -0.5 to 0.5 x, -0.5 to 0.5 y (center quarter)
    auto entry = std::make_shared<MouseClickEventHandlerEntry>(SDL_MOUSEBUTTONDOWN, -0.5f, 0.5f, 1.0f, 1.0f, cb, ctx);
    EventHandler& eh = EventHandler::get_instance();
    eh.register_entry(entry);

    auto [w, h] = ctx.get_size();

    SDL_Event ev;
    ev.type = SDL_MOUSEBUTTONDOWN;
    ev.button.windowID = ctx.window_id;
    ev.button.button = SDL_BUTTON_LEFT;
    ev.button.x = w / 2; // center, inside
    ev.button.y = h / 2;

    bool handled = eh.handle_event(ev);
    REQUIRE(handled);
    REQUIRE(called);

    called = false;
    ev.button.x = 0; // left edge, outside if region is center
    ev.button.y = 0;
    handled = eh.handle_event(ev);
    REQUIRE_FALSE(handled);
    REQUIRE_FALSE(called);

    // Cleanup
    eh.unregister_entry(entry);
}

// Add more tests for MouseMotion, MouseEnterLeave similarly

// For MouseEnterLeave, need multiple events to simulate movement

TEST_CASE("MouseEnterLeaveEventHandlerEntry", "[event_handler][mouse]") {
    SDLInitGuard sdl_guard;
    DummyRenderableEntity dummy(800, 600, false);
    RenderContext ctx = dummy.get_render_context();

    bool entered = false;
    auto enter_cb = [&entered](const SDL_Event&) { entered = true; return true; };

    bool left = false;
    auto leave_cb = [&left](const SDL_Event&) { left = true; return true; };

    auto enter_entry = std::make_shared<MouseEnterLeaveEventHandlerEntry>(-1.0f, 1.0f, 2.0f, 2.0f, MouseEnterLeaveEventHandlerEntry::Mode::ENTER, enter_cb, ctx);
    auto leave_entry = std::make_shared<MouseEnterLeaveEventHandlerEntry>(-1.0f, 1.0f, 2.0f, 2.0f, MouseEnterLeaveEventHandlerEntry::Mode::LEAVE, leave_cb, ctx);

    EventHandler& eh = EventHandler::get_instance();
    eh.register_entry(enter_entry);
    eh.register_entry(leave_entry);

    auto [w, h] = ctx.get_size();

    SDL_Event motion;
    motion.type = SDL_MOUSEMOTION;
    motion.motion.windowID = ctx.window_id;

    // Start outside
    motion.motion.x = -10;
    motion.motion.y = -10;
    eh.handle_event(motion);

    // Move inside
    motion.motion.x = w / 2;
    motion.motion.y = h / 2;
    eh.handle_event(motion);
    REQUIRE(entered);
    REQUIRE_FALSE(left);

    entered = false;

    // Move outside
    motion.motion.x = w + 10;
    motion.motion.y = h + 10;
    eh.handle_event(motion);
    REQUIRE(left);
    REQUIRE_FALSE(entered);

    // Cleanup
    eh.unregister_entry(enter_entry);
    eh.unregister_entry(leave_entry);
}

// Add tests for GPIO if implemented, else skip

// Assume GlobalMouseUpEventHandlerEntry exists and test similarly if definition known

#endif // EVENT_HANDLER_TEST_CPP
