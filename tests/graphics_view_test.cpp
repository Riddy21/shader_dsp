#include "catch2/catch_all.hpp"
#include <SDL2/SDL.h>
#include <thread>
#include <chrono>

#define private public
#undef protected

#include "graphics_core/graphics_view.h"
#include "graphics_core/graphics_component.h"
#include "engine/event_handler.h"

#undef private
#define protected protected

class MockGraphicsComponent : public GraphicsComponent {
public:
    bool initialize_called = false;
    bool render_called = false;
    bool register_called = false;
    bool unregister_called = false;
    RenderContext passed_render_context;

    MockGraphicsComponent() : GraphicsComponent(0.0f, 0.0f, 1.0f, 1.0f, nullptr, RenderContext()) {}

    bool initialize() override {
        initialize_called = true;
        return GraphicsComponent::initialize();
    }

    void render_content() override {
        render_called = true;
    }

    void register_event_handlers(EventHandler* event_handler) override {
        register_called = true;
        GraphicsComponent::register_event_handlers(event_handler);
    }

    void unregister_event_handlers() override {
        unregister_called = true;
        GraphicsComponent::unregister_event_handlers();
    }
};

TEST_CASE("GraphicsView add_component", "[graphics_view]") {
    EventHandler& event_handler = EventHandler::get_instance();
    RenderContext render_context;
    render_context.window_id = 1;

    GraphicsView view;
    view.initialize(event_handler, render_context);

    MockGraphicsComponent* component = new MockGraphicsComponent();
    view.add_component(component);

    REQUIRE(view.m_components.size() == 1);
    REQUIRE(view.m_components[0].get() == component);
    REQUIRE(component->m_render_context.window_id == 1);
    // Since view is initialized but not entered, no registration yet
    REQUIRE(!component->register_called);
}

TEST_CASE("GraphicsView add_component after enter", "[graphics_view]") {
    EventHandler& event_handler = EventHandler::get_instance();
    RenderContext render_context;
    render_context.window_id = 1;

    GraphicsView view;
    view.initialize(event_handler, render_context);
    view.on_enter(); // This registers

    MockGraphicsComponent* component = new MockGraphicsComponent();
    view.add_component(component);

    REQUIRE(view.m_components.size() == 1);
    REQUIRE(component->m_render_context.window_id == 1);
    REQUIRE(component->register_called); // Should register since view is entered
}

TEST_CASE("GraphicsView remove_component", "[graphics_view]") {
    EventHandler& event_handler = EventHandler::get_instance();
    RenderContext render_context;
    render_context.window_id = 1;

    GraphicsView view;
    view.initialize(event_handler, render_context);

    MockGraphicsComponent* component = new MockGraphicsComponent();
    view.add_component(component);
    view.remove_component(component);

    REQUIRE(view.m_components.empty());
}

TEST_CASE("GraphicsView remove_component after enter", "[graphics_view]") {
    EventHandler& event_handler = EventHandler::get_instance();
    RenderContext render_context;
    render_context.window_id = 1;

    GraphicsView view;
    view.initialize(event_handler, render_context);
    view.on_enter();

    MockGraphicsComponent* component = new MockGraphicsComponent();
    view.add_component(component);
    view.remove_component(component);

    REQUIRE(view.m_components.empty());
    REQUIRE(component->unregister_called); // Should unregister
}

TEST_CASE("GraphicsView set_render_context", "[graphics_view]") {
    EventHandler& event_handler = EventHandler::get_instance();
    RenderContext old_context;
    old_context.window_id = 1;
    RenderContext new_context;
    new_context.window_id = 2;

    GraphicsView view;
    view.initialize(event_handler, old_context);

    MockGraphicsComponent* component = new MockGraphicsComponent();
    view.add_component(component);

    view.set_render_context(new_context);

    REQUIRE(view.m_render_context.window_id == 2);
    REQUIRE(component->m_render_context.window_id == 2);
}

TEST_CASE("GraphicsView set_event_handler", "[graphics_view]") {
    EventHandler& old_handler = EventHandler::get_instance();
    EventHandler new_handler; // Assuming we can create another for testing, but since singleton, might need adjustment

    // Note: EventHandler is singleton, so this might not work directly. For testing, perhaps skip or mock.
    // Assuming we can test with same handler or adjust.

    GraphicsView view;
    RenderContext render_context;
    view.initialize(old_handler, render_context);
    view.on_enter();

    view.set_event_handler(new_handler);

    REQUIRE(view.m_event_handler == &new_handler);
    REQUIRE(!view.m_event_handlers_registered); // Should have unregistered
}

TEST_CASE("GraphicsView on_enter and on_exit", "[graphics_view]") {
    EventHandler& event_handler = EventHandler::get_instance();
    RenderContext render_context;
    render_context.window_id = 1;

    GraphicsView view;
    view.initialize(event_handler, render_context);

    MockGraphicsComponent* component = new MockGraphicsComponent();
    view.add_component(component);

    view.on_enter();

    REQUIRE(view.m_event_handlers_registered);
    REQUIRE(component->register_called);

    view.on_exit();

    REQUIRE(!view.m_event_handlers_registered);
    REQUIRE(component->unregister_called);
}

TEST_CASE("GraphicsView render", "[graphics_view]") {
    EventHandler& event_handler = EventHandler::get_instance();
    RenderContext render_context;

    GraphicsView view;
    view.initialize(event_handler, render_context);

    MockGraphicsComponent* component = new MockGraphicsComponent();
    view.add_component(component);

    view.render();

    REQUIRE(component->render_called);
}

TEST_CASE("GraphicsView initializes components", "[graphics_view]") {
    EventHandler& event_handler = EventHandler::get_instance();
    RenderContext render_context;

    GraphicsView view;

    MockGraphicsComponent* component = new MockGraphicsComponent();
    view.add_component(component);

    view.initialize(event_handler, render_context);

    REQUIRE(component->initialize_called);
}
