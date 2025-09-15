#ifndef EVENT_LOOP_TEST_CPP
#define EVENT_LOOP_TEST_CPP

#include "catch2/catch_all.hpp"

#include <SDL2/SDL.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <array>

#include "engine/event_loop.h"
#include "engine/event_handler.h"
#include "engine/renderable_entity.h"
#include "test_sdl_manager.h"

// Mock RenderableEntity to track method calls
namespace {
class DummyRenderableEntity : public IRenderableEntity {

public:

    struct Colour { float r, g, b, a; };

    DummyRenderableEntity(const Colour & clear_colour, unsigned int w = 64, unsigned int h = 64, bool visible = false, const std::string& title = "Mock")

        : m_clear_colour(clear_colour), render_count(0), present_count(0), activate_count(0), unactivate_count(0), ready(true) {

        REQUIRE(initialize_sdl(w, h, title, visible ? SDL_WINDOW_SHOWN : SDL_WINDOW_HIDDEN, visible));

    }

    bool is_ready() override { return ready; }

    void render() override {
        render_count++;
        activate_render_context();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, get_render_context().get_size().first, get_render_context().get_size().second);
        glClearColor(m_clear_colour.r, m_clear_colour.g, m_clear_colour.b, m_clear_colour.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        update_render_fps();

    }

    void present() override {
        present_count++;
        IRenderableEntity::present();
    }

    void activate_render_context() override {
        activate_count++;
        IRenderableEntity::activate_render_context();
    }

    void unactivate_render_context() override {
        unactivate_count++;
        IRenderableEntity::unactivate_render_context();
    }

    std::atomic<int> render_count;
    std::atomic<int> present_count;
    std::atomic<int> activate_count;
    std::atomic<int> unactivate_count;
    bool ready;

private:

    Colour m_clear_colour;

};
}  // anonymous namespace

// After MockRenderableEntity, add a custom entry for testing

class TestEventHandlerEntry : public EventHandlerEntry {
public:
    TestEventHandlerEntry(Uint32 type, bool should_handle, std::atomic<bool>& handled_flag, RenderContext ctx = RenderContext())
        : EventHandlerEntry(ctx, [this, &handled_flag](const SDL_Event&) {
            handled_flag = true;
            return this->should_handle;
          }),
          event_type(type), should_handle(should_handle) {}

    bool matches(const SDL_Event& event) override {
        return event.type == event_type;
    }

private:
    Uint32 event_type;
    bool should_handle;
};

static std::array<float, 4> read_center_pixel(IRenderableEntity & entity) {

    entity.activate_render_context();

    auto [w, h] = entity.get_render_context().get_size();

    GLint x = static_cast<GLint>(w / 2);

    GLint y = static_cast<GLint>(h / 2);

    glFinish();

    std::array<unsigned char,4> pixelBytes{};

    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixelBytes.data());

    std::array<float,4> pixel{};

    for(int i=0;i<4;++i){ pixel[i]= pixelBytes[i]/255.0f; }

    entity.unactivate_render_context();

    return pixel;

}

TEST_CASE("EventLoop singleton", "[event_loop]") {
    auto& el1 = EventLoop::get_instance();
    auto& el2 = EventLoop::get_instance();
    REQUIRE(&el1 == &el2);
}

TEST_CASE("EventLoop add items and handlers", "[event_loop]") {
    TestSDLGuard sdl_guard(SDL_INIT_EVERYTHING);
    EventLoop& el = EventLoop::get_instance();

    DummyRenderableEntity* entity = new DummyRenderableEntity({0.0f, 0.0f, 0.0f, 1.0f});
    el.add_loop_item(entity);

    // No direct way to verify, but if no crash, assume ok
    el.remove_loop_item(entity);
}

TEST_CASE("EventLoop terminates on SDL_QUIT", "[event_loop]") {
    TestSDLGuard sdl_guard(SDL_INIT_EVERYTHING);
    EventLoop& el = EventLoop::get_instance();

    std::thread terminator([&el]() {

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        el.terminate();

    });

    el.run_loop();

    terminator.join();
    // If joined without deadlock, test passes
}

TEST_CASE("EventLoop renders on events if handled", "[event_loop]") {
    TestSDLGuard sdl_guard(SDL_INIT_EVERYTHING);
    EventLoop& el = EventLoop::get_instance();

    DummyRenderableEntity* entity1 = new DummyRenderableEntity({1.0f, 0.0f, 0.0f, 1.0f});
    DummyRenderableEntity* entity2 = new DummyRenderableEntity({0.0f, 1.0f, 0.0f, 1.0f});
    el.add_loop_item(entity1);
    el.add_loop_item(entity2);

    std::atomic<bool> handled{false};
    auto entry = std::make_shared<TestEventHandlerEntry>(SDL_USEREVENT, true, handled);
    EventHandler::get_instance().register_entry(entry);

    std::thread terminator([&el]() {

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Push an event
        SDL_Event ev;
        ev.type = SDL_USEREVENT;
        SDL_PushEvent(&ev);

        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Allow processing

        el.terminate();

    });

    el.run_loop();

    terminator.join();

    el.remove_loop_item(entity1);
    el.remove_loop_item(entity2);

    // Check that handle was called and renders happened
    REQUIRE(handled.load());
    REQUIRE(entity1->render_count.load() > 0);
    REQUIRE(entity2->render_count.load() > 0);
    REQUIRE(entity1->present_count.load() > 0);
    REQUIRE(entity2->present_count.load() > 0);
}

TEST_CASE("EventLoop context isolation", "[event_loop]") {
    TestSDLGuard sdl_guard(SDL_INIT_EVERYTHING);
    EventLoop& el = EventLoop::get_instance();

    DummyRenderableEntity* entity1 = new DummyRenderableEntity({1.0f, 0.0f, 0.0f, 1.0f}, 64, 64, false, "Entity1");
    DummyRenderableEntity* entity2 = new DummyRenderableEntity({0.0f, 1.0f, 0.0f, 1.0f}, 64, 64, false, "Entity2");
    el.add_loop_item(entity1);
    el.add_loop_item(entity2);

    std::atomic<bool> handled{false};
    auto entry = std::make_shared<TestEventHandlerEntry>(SDL_USEREVENT, true, handled);
    EventHandler::get_instance().register_entry(entry);

    std::thread terminator([&el]() {

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Push an event to trigger render
        SDL_Event ev;
        ev.type = SDL_USEREVENT;
        SDL_PushEvent(&ev);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        el.terminate();

    });

    el.run_loop();

    terminator.join();

    // Render each again and read pixels
    const Uint32 test_duration_ms = 1000;
    const Uint32 start_time = SDL_GetTicks();
    while (SDL_GetTicks() - start_time < test_duration_ms) {
        entity1->activate_render_context();
        entity1->render();
        entity1->present();
        entity1->unactivate_render_context();
        auto px1 = read_center_pixel(*entity1);
        REQUIRE(px1[0] == Catch::Approx(1.0f).margin(0.01f));
        REQUIRE(px1[1] == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(px1[2] == Catch::Approx(0.0f).margin(0.01f));

        entity2->activate_render_context();
        entity2->render();
        entity2->present();
        entity2->unactivate_render_context();
        auto px2 = read_center_pixel(*entity2);
        REQUIRE(px2[0] == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(px2[1] == Catch::Approx(1.0f).margin(0.01f));
        REQUIRE(px2[2] == Catch::Approx(0.0f).margin(0.01f));
    }

    // Check activation counts to ensure isolation
    REQUIRE(entity1->activate_count.load() > 0);
    REQUIRE(entity2->activate_count.load() > 0);

    el.remove_loop_item(entity1);
    el.remove_loop_item(entity2);
}

#endif // EVENT_LOOP_TEST_CPP
