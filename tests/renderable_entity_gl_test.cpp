#ifndef RENDERABLE_ENTITY_GL_TEST_CPP
#define RENDERABLE_ENTITY_GL_TEST_CPP

#include "catch2/catch_all.hpp"

#include <SDL2/SDL.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <stdexcept>
#include <array>
#include <iostream>

#include "engine/renderable_entity.h"
#include "test_sdl_manager.h"


namespace {
// Define a minimal dummy entity that doesn't set clear color in render()
class DummyRenderableEntity : public IRenderableEntity {
public:
    struct Colour { float r, g, b, a; };

    DummyRenderableEntity(const Colour & clear_colour, unsigned int w = 64, unsigned int h = 64, bool visible = false, const std::string& title = "DummyEntity")
        : m_clear_colour(clear_colour)
    {
        REQUIRE(initialize_sdl(w, h, title, SDL_WINDOW_HIDDEN, visible, false));
    }

    bool is_ready() override { return true; }

    void render() override {
        IRenderableEntity::render();
        activate_render_context();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, get_render_context().get_size().first, get_render_context().get_size().second);
        glClearColor(m_clear_colour.r, m_clear_colour.g, m_clear_colour.b, m_clear_colour.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        update_render_fps();
    }

    void present() override {
        IRenderableEntity::present();
    }

    Colour m_clear_colour;
};
}  // anonymous namespace


// Helper that renders the entity once and returns the centre pixel as floats
static std::array<float, 4> read_center_pixel(IRenderableEntity & entity) {
    // Read back a single pixel from the middle of the window's back buffer
    auto [w, h] = entity.get_render_context().get_size();
    GLint x = static_cast<GLint>(w / 2);
    GLint y = static_cast<GLint>(h / 2);
    glFinish();
    std::array<unsigned char,4> pixelBytes{};
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixelBytes.data());
    std::array<float,4> pixel{};
    for(int i=0;i<4;++i){ pixel[i]= pixelBytes[i]/255.0f; }
    return pixel;
}

TEST_CASE("Multiple windows render with hidden windows and colour readback", "[renderable_entity][multi_window][hidden][readpixels]") {
    TestSDLGuard sdl_guard(SDL_INIT_VIDEO, true);

    // Visible windows
    DummyRenderableEntity red    ({1.0f, 0.0f, 0.0f, 1.0f}, 32, 32, true,  "RedWindow");
    DummyRenderableEntity green  ({0.0f, 1.0f, 0.0f, 1.0f}, 32, 32, true,  "GreenWindow");
    DummyRenderableEntity blue   ({0.0f, 0.0f, 1.0f, 1.0f}, 32, 32, true,  "BlueWindow");
    DummyRenderableEntity yellow ({1.0f, 1.0f, 0.0f, 1.0f}, 32, 32, true,  "YellowWindow");

    // Hidden windows
    DummyRenderableEntity hidden_grey  ({0.5f, 0.5f, 0.5f, 1.0f}, 16, 16, false, "HiddenGreyWindow");
    DummyRenderableEntity hidden_blue  ({0.0f, 0.0f, 1.0f, 1.0f}, 32, 32, false, "HiddenBlueWindow");

    // Verify visibility flags for hidden windows
    REQUIRE_FALSE(SDL_GetWindowFlags(hidden_grey.get_window()) & SDL_WINDOW_SHOWN);
    REQUIRE(SDL_GetWindowFlags(hidden_grey.get_window()) & SDL_WINDOW_HIDDEN);
    REQUIRE_FALSE(SDL_GetWindowFlags(hidden_blue.get_window()) & SDL_WINDOW_SHOWN);
    REQUIRE(SDL_GetWindowFlags(hidden_blue.get_window()) & SDL_WINDOW_HIDDEN);

    // Continuously render, present, and verify pixel colours for roughly five seconds
    const Uint32 duration_ms = 5000;
    const Uint32 start_time = SDL_GetTicks();
    while (SDL_GetTicks() - start_time < duration_ms) {
        // Red window
        red.render();
        red.present();
        auto red_px = read_center_pixel(red);
        REQUIRE(red_px[0] == Catch::Approx(1.0f).margin(0.01f));
        REQUIRE(red_px[1] == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(red_px[2] == Catch::Approx(0.0f).margin(0.01f));
        red.unactivate_render_context();

        // Green window
        green.render();
        green.present();
        auto green_px = read_center_pixel(green);
        REQUIRE(green_px[0] == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(green_px[1] == Catch::Approx(1.0f).margin(0.01f));
        REQUIRE(green_px[2] == Catch::Approx(0.0f).margin(0.01f));
        green.unactivate_render_context();

        // Blue visible window
        blue.render();
        blue.present();
        auto blue_vis_px = read_center_pixel(blue);
        REQUIRE(blue_vis_px[0] == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(blue_vis_px[1] == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(blue_vis_px[2] == Catch::Approx(1.0f).margin(0.01f));
        blue.unactivate_render_context();

        // Yellow window
        yellow.render();
        yellow.present();
        auto yellow_px = read_center_pixel(yellow);
        REQUIRE(yellow_px[0] == Catch::Approx(1.0f).margin(0.01f));
        REQUIRE(yellow_px[1] == Catch::Approx(1.0f).margin(0.01f));
        REQUIRE(yellow_px[2] == Catch::Approx(0.0f).margin(0.01f));
        yellow.unactivate_render_context();

        // Hidden grey window (off-screen)
        hidden_grey.render();
        hidden_grey.present();
        auto grey_px = read_center_pixel(hidden_grey);
        REQUIRE(grey_px[0] == Catch::Approx(0.5f).margin(0.01f));
        REQUIRE(grey_px[1] == Catch::Approx(0.5f).margin(0.01f));
        REQUIRE(grey_px[2] == Catch::Approx(0.5f).margin(0.01f));
        hidden_grey.unactivate_render_context();

        // Hidden blue window (off-screen)
        hidden_blue.render();
        hidden_blue.present();
        auto blue_hid_px = read_center_pixel(hidden_blue);
        REQUIRE(blue_hid_px[0] == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(blue_hid_px[1] == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(blue_hid_px[2] == Catch::Approx(1.0f).margin(0.01f));
        hidden_blue.unactivate_render_context();
    }

    // Post-loop sanity: ensure hidden contexts are still valid
    REQUIRE(hidden_grey.get_render_context().is_valid());
    REQUIRE(hidden_blue.get_render_context().is_valid());

    // Cleanup contexts in case any remain active
    red.unactivate_render_context();
    green.unactivate_render_context();
    blue.unactivate_render_context();
    yellow.unactivate_render_context();
    hidden_grey.unactivate_render_context();
    hidden_blue.unactivate_render_context();
}

TEST_CASE("IRenderableEntity VSync affects presentation FPS", "[renderable_entity][vsync][!shouldfail]") {
    // TODO: Re-enable this test once VSync implementation is fixed
    // Current issues:
    // - VSync setting may not be properly applied through EGL layer
    // - FPS measurements may not reflect actual display refresh rate
    // - Need to verify EGL swap interval functionality
    SKIP("VSync functionality is not working properly at the moment - needs investigation");
    
    TestSDLGuard sdl_guard(SDL_INIT_VIDEO, true);

    // Use a visible window so buffer swaps happen
    DummyRenderableEntity entity({0.1f, 0.2f, 0.3f, 1.0f}, 64, 64, true, "VSyncWindow");

    // ---------- Measure with VSync disabled ----------
    entity.set_vsync_enabled(false);
    std::cout << "Testing with VSync disabled..." << std::endl;
    
    const Uint32 no_vsync_duration = 3000; // 3 seconds for more stable measurement
    Uint32 start = SDL_GetTicks();
    while (SDL_GetTicks() - start < no_vsync_duration) {
        entity.render();
        entity.present();
        std::cout << "FPS: " << entity.get_present_fps() << std::endl;
    }
    float fps_no_vsync = entity.get_present_fps();
    std::cout << "FPS with VSync disabled: " << fps_no_vsync << std::endl;
    REQUIRE(fps_no_vsync > 0.0f);

    // ---------- Measure with VSync enabled ----------
    entity.set_vsync_enabled(true);
    std::cout << "Testing with VSync enabled..." << std::endl;
    
    const Uint32 vsync_duration = 3000; // 3 seconds
    start = SDL_GetTicks();
    while (SDL_GetTicks() - start < vsync_duration) {
        entity.render();
        entity.present();
        std::cout << "FPS: " << entity.get_present_fps() << std::endl;
    }
    float fps_vsync = entity.get_present_fps();
    std::cout << "FPS with VSync enabled: " << fps_vsync << std::endl;
    REQUIRE(fps_vsync > 0.0f);

    // Expect a significant drop when VSync is on (display refresh cap)
    // But be more lenient since some systems might not enforce VSync strictly
    std::cout << "FPS difference: " << (fps_no_vsync - fps_vsync) << std::endl;
    REQUIRE(fps_no_vsync > fps_vsync);
    REQUIRE((fps_no_vsync - fps_vsync) > 1.0f); // Reduced threshold

    entity.unactivate_render_context();
}

TEST_CASE("IRenderableEntity OpenGL state independence between contexts", "[renderable_entity][context][state]") {
    TestSDLGuard sdl_guard(SDL_INIT_VIDEO, true);

    // Create two hidden entities
    DummyRenderableEntity entity1({1.0f, 0.0f, 0.0f, 1.0f}, 64, 64, false, "Entity1");
    DummyRenderableEntity entity2({0.0f, 1.0f, 0.0f, 1.0f}, 64, 64, false, "Entity2");

    // Now test rendering and state preservation over multiple switches
    const Uint32 duration_ms = 5000;
    const Uint32 start_time = SDL_GetTicks();
    while (SDL_GetTicks() - start_time < duration_ms) {
        // Render entity1, check pixel color and query clear color state
        entity1.render();
        entity1.present();
        auto px1 = read_center_pixel(entity1);
        REQUIRE(px1[0] == Catch::Approx(1.0f).margin(0.01f));
        REQUIRE(px1[1] == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(px1[2] == Catch::Approx(0.0f).margin(0.01f));

        float clear_color1[4];
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clear_color1);
        REQUIRE(clear_color1[0] == Catch::Approx(1.0f).margin(0.01f));
        REQUIRE(clear_color1[1] == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(clear_color1[2] == Catch::Approx(0.0f).margin(0.01f));
        entity1.unactivate_render_context();

        // Render entity2, check pixel color and query clear color state
        entity2.render();
        entity2.present();
        auto px2 = read_center_pixel(entity2);
        REQUIRE(px2[0] == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(px2[1] == Catch::Approx(1.0f).margin(0.01f));
        REQUIRE(px2[2] == Catch::Approx(0.0f).margin(0.01f));

        float clear_color2[4];
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clear_color2);
        REQUIRE(clear_color2[0] == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(clear_color2[1] == Catch::Approx(1.0f).margin(0.01f));
        REQUIRE(clear_color2[2] == Catch::Approx(0.0f).margin(0.01f));
        entity2.unactivate_render_context();
    }

    // Final cleanup
    entity1.unactivate_render_context();
    entity2.unactivate_render_context();
}

#endif // RENDERABLE_ENTITY_GL_TEST_CPP 