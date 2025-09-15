#include "catch2/catch_all.hpp"
#include <SDL2/SDL.h>
#include <GLES3/gl3.h>

#include "graphics_core/graphics_component.h"
#include "engine/event_handler.h"
#include "graphics_components/button_component.h"
#include "test_sdl_manager.h"

// Mock for child components to track calls
class MockGraphicsComponent : public GraphicsComponent {
public:
    bool initialize_called = false;
    bool render_content_called = false;
    bool register_called = false;
    bool unregister_called = false;
    bool draw_outline_called = false;

    MockGraphicsComponent(float x, float y, float w, float h, EventHandler* eh = nullptr, RenderContext ctx = RenderContext())
        : GraphicsComponent(x, y, w, h, eh, ctx) {}

    bool initialize() override {
        initialize_called = true;
        return GraphicsComponent::initialize();
    }

    void render_content() override {
        render_content_called = true;
        GraphicsComponent::render_content();
    }

    void register_event_handlers(EventHandler* event_handler) override {
        register_called = true;
        GraphicsComponent::register_event_handlers(event_handler);
    }

    void unregister_event_handlers() override {
        unregister_called = true;
        GraphicsComponent::unregister_event_handlers();
    }

    void draw_outline() override {
        draw_outline_called = true;
        GraphicsComponent::draw_outline();
    }
};

// Simple EGL setup helper
struct EGLSetup {
    SDL_Window* window;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLConfig config;

    EGLSetup(int w, int h) {
        window = SDL_CreateWindow("Button Test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
        REQUIRE(window != nullptr);

        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        REQUIRE(display != EGL_NO_DISPLAY);

        EGLint major, minor;
        REQUIRE(eglInitialize(display, &major, &minor));

        const EGLint attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_NONE
        };
        EGLint numConfigs;
        REQUIRE(eglChooseConfig(display, attribs, &config, 1, &numConfigs));
        REQUIRE(numConfigs > 0);

        surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)SDL_GetWindowSurface(window), NULL);
        REQUIRE(surface != EGL_NO_SURFACE);

        const EGLint ctx_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctx_attribs);
        REQUIRE(context != EGL_NO_CONTEXT);

        REQUIRE(eglMakeCurrent(display, surface, surface, context));
    }

    ~EGLSetup() {
        eglDestroyContext(display, context);
        eglDestroySurface(display, surface);
        eglTerminate(display);
        SDL_DestroyWindow(window);
    }
};

TEST_CASE("GraphicsComponent constructor", "[graphics_component]") {
    EventHandler eh;
    RenderContext ctx;
    ctx.window_id = 1;

    GraphicsComponent comp(0.1f, 0.2f, 0.3f, 0.4f, &eh, ctx);

    REQUIRE(comp.m_x == 0.1f);
    REQUIRE(comp.m_y == 0.2f);
    REQUIRE(comp.m_width == 0.3f);
    REQUIRE(comp.m_height == 0.4f);
    REQUIRE(comp.m_event_handler == &eh);
    REQUIRE(comp.m_render_context.window_id == 1);
    REQUIRE(!comp.m_initialized);
    REQUIRE(comp.m_event_handlers_registered); // Registered in constructor if eh provided
}

TEST_CASE("GraphicsComponent initialize", "[graphics_component]") {
    MockGraphicsComponent comp(0.0f, 0.0f, 1.0f, 1.0f);

    MockGraphicsComponent* child = new MockGraphicsComponent(0.0f, 0.0f, 0.5f, 0.5f);
    comp.add_child(child);

    REQUIRE(comp.initialize());
    REQUIRE(comp.m_initialized);
    REQUIRE(child->initialize_called);
}

TEST_CASE("GraphicsComponent render skips if zero dimensions", "[graphics_component]") {
    MockGraphicsComponent comp(0.0f, 0.0f, 0.0f, 0.0f);

    comp.render();
    REQUIRE(!comp.render_content_called);
}

TEST_CASE("GraphicsComponent render calls content and children", "[graphics_component]") {
    MockGraphicsComponent comp(0.0f, 0.0f, 1.0f, 1.0f);

    MockGraphicsComponent* child = new MockGraphicsComponent(0.0f, 0.0f, 0.5f, 0.5f);
    comp.add_child(child);

    comp.render();

    REQUIRE(comp.render_content_called);
    REQUIRE(child->render_content_called);
}

TEST_CASE("GraphicsComponent render draws outline if enabled", "[graphics_component]") {
    MockGraphicsComponent comp(0.0f, 0.0f, 1.0f, 1.0f);
    comp.m_show_outline = true;

    comp.render();
    REQUIRE(comp.draw_outline_called);
}

TEST_CASE("GraphicsComponent event handling", "[graphics_component]") {
    EventHandler eh;

    MockGraphicsComponent comp(0.0f, 0.0f, 1.0f, 1.0f, &eh);

    REQUIRE(comp.m_event_handlers_registered);

    MockGraphicsComponent* child = new MockGraphicsComponent(0.0f, 0.0f, 0.5f, 0.5f);
    comp.add_child(child);

    REQUIRE(child->register_called);

    comp.unregister_event_handlers();
    REQUIRE(!comp.m_event_handlers_registered);
    REQUIRE(child->unregister_called);
}

TEST_CASE("GraphicsComponent set_position propagates to children", "[graphics_component]") {
    MockGraphicsComponent comp(0.0f, 0.0f, 1.0f, 1.0f);

    MockGraphicsComponent* child = new MockGraphicsComponent(0.1f, 0.1f, 0.5f, 0.5f);
    comp.add_child(child);

    comp.set_position(0.5f, 0.6f);

    REQUIRE(comp.m_x == 0.5f);
    REQUIRE(comp.m_y == 0.6f);
    REQUIRE(child->m_x == 0.1f + 0.5f);
    REQUIRE(child->m_y == 0.1f + 0.6f);
}

TEST_CASE("GraphicsComponent set_dimensions propagates to children", "[graphics_component]") {
    MockGraphicsComponent comp(0.0f, 0.0f, 1.0f, 1.0f);

    MockGraphicsComponent* child = new MockGraphicsComponent(0.1f, 0.1f, 0.5f, 0.5f);
    comp.add_child(child);

    comp.set_dimensions(2.0f, 3.0f);

    REQUIRE(comp.m_width == 2.0f);
    REQUIRE(comp.m_height == 3.0f);
    float width_ratio = 2.0f / 1.0f;
    float height_ratio = 3.0f / 1.0f;
    REQUIRE(child->m_width == 0.5f * width_ratio);
    REQUIRE(child->m_height == 0.5f * height_ratio);
    // Also check position adjusted
    REQUIRE(child->m_x == 0.0f + (0.1f - 0.0f) * width_ratio);
    REQUIRE(child->m_y == 0.0f + (0.1f - 0.0f) * height_ratio);
}

TEST_CASE("GraphicsComponent add_remove_child", "[graphics_component]") {
    EventHandler eh;

    MockGraphicsComponent comp(0.0f, 0.0f, 1.0f, 1.0f, &eh);
    comp.m_event_handlers_registered = true;

    MockGraphicsComponent* child = new MockGraphicsComponent(0.0f, 0.0f, 0.5f, 0.5f);
    comp.add_child(child);

    REQUIRE(comp.get_child_count() == 1);
    REQUIRE(comp.get_child(0) == child);
    REQUIRE(child->register_called); // Since parent registered

    comp.remove_child(child);
    REQUIRE(comp.get_child_count() == 0);
    REQUIRE(child->unregister_called);
}

TEST_CASE("GraphicsComponent set_render_context propagates", "[graphics_component]") {
    MockGraphicsComponent comp(0.0f, 0.0f, 1.0f, 1.0f);

    MockGraphicsComponent* child = new MockGraphicsComponent(0.0f, 0.0f, 0.5f, 0.5f);
    comp.add_child(child);

    RenderContext new_ctx;
    new_ctx.window_id = 2;
    comp.set_render_context(new_ctx);

    REQUIRE(comp.m_render_context.window_id == 2);
    REQUIRE(child->m_render_context.window_id == 2);
}

TEST_CASE("GraphicsComponent set_outline_color", "[graphics_component]") {
    GraphicsComponent comp(0.0f, 0.0f, 1.0f, 1.0f);

    comp.set_outline_color(0.1f, 0.2f, 0.3f, 0.4f);

    REQUIRE(comp.m_outline_color[0] == 0.1f);
    REQUIRE(comp.m_outline_color[1] == 0.2f);
    REQUIRE(comp.m_outline_color[2] == 0.3f);
    REQUIRE(comp.m_outline_color[3] == 0.4f);
}
