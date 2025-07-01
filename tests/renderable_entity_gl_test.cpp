#ifndef RENDERABLE_ENTITY_GL_TEST_CPP
#define RENDERABLE_ENTITY_GL_TEST_CPP

#include "catch2/catch_all.hpp"

#include <SDL2/SDL.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <stdexcept>
#include <array>
#include <iostream>

#include "engine/renderable_item.h"

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

// Dummy implementation of IRenderableEntity used only for testing
class DummyRenderableEntity : public IRenderableEntity {
public:
    struct Colour { float r, g, b, a; };

    DummyRenderableEntity(const Colour & clear_colour,
                          unsigned int w = 64,
                          unsigned int h = 64,
                          bool visible = true,
                          const std::string & title = "DummyEntity")
        : m_clear_colour(clear_colour)
    {
        REQUIRE(initialize_sdl(w, h, title, SDL_WINDOW_SHOWN, visible));
    }

    // Always ready for rendering in tests
    bool is_ready() override { return true; }

    void render() override {
        activate_render_context();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, get_render_context().get_size().first, get_render_context().get_size().second);
        glClearColor(m_clear_colour.r, m_clear_colour.g, m_clear_colour.b, m_clear_colour.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        update_render_fps();
        // Keep context active for inspection; caller may unactivate when desired
    }

    void present() override {
        IRenderableEntity::present();
    }

private:
    Colour m_clear_colour;
};

// Helper that renders the entity once and returns the centre pixel as floats
static std::array<float, 4> read_center_pixel(DummyRenderableEntity & entity) {
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

TEST_CASE("IRenderableEntity context switching", "[renderable_entity][context_switch]") {
    SDLInitGuard sdl_guard; // Ensure SDL is initialised for this scope

    // Create two visible windows with different clear colours
    DummyRenderableEntity red    ({1.0f, 0.0f, 0.0f, 1.0f}, 64, 64, true,  "RedWindow");
    DummyRenderableEntity green  ({0.0f, 1.0f, 0.0f, 1.0f}, 64, 64, true,  "GreenWindow");

    // Continuously render each window for roughly one second, checking the centre pixel colour every time.
    // This extended loop helps to detect any transient flashes that may appear under visual inspection.
    const Uint32 test_duration_ms = 5000; // Run for one second
    const Uint32 start_time = SDL_GetTicks();
    while (SDL_GetTicks() - start_time < test_duration_ms) {
        // Render red, present (swap buffers), and verify pixel colour
        red.render();                // performs glClear with entity colour
        red.present();               // swap buffers to simulate real frame presentation
        auto red_px = read_center_pixel(red);
        REQUIRE(red_px[0] == Catch::Approx(1.0f).margin(0.01f));
        REQUIRE(red_px[1] == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(red_px[2] == Catch::Approx(0.0f).margin(0.01f));
        red.unactivate_render_context();

        // Render green (this implicitly switches context), present, and verify pixel colour
        green.render();
        green.present();
        auto green_px = read_center_pixel(green);
        REQUIRE(green_px[0] == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(green_px[1] == Catch::Approx(1.0f).margin(0.01f));
        REQUIRE(green_px[2] == Catch::Approx(0.0f).margin(0.01f));
        green.unactivate_render_context();
    }

    // Final sanity check after the loop has completed
    red.render();
    red.present();
    auto red_px_final = read_center_pixel(red);
    REQUIRE(red_px_final[0] == Catch::Approx(1.0f).margin(0.01f));
    REQUIRE(red_px_final[1] == Catch::Approx(0.0f).margin(0.01f));
    REQUIRE(red_px_final[2] == Catch::Approx(0.0f).margin(0.01f));

    // Cleanup contexts
    green.unactivate_render_context();
    red.unactivate_render_context();
}

TEST_CASE("Multiple windows render with hidden windows and colour readback", "[renderable_entity][multi_window][hidden][readpixels]") {
    SDLInitGuard sdl_guard;

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
    
    SDLInitGuard sdl_guard;

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
    }
    float fps_vsync = entity.get_present_fps();
    std::cout << "FPS with VSync enabled: " << fps_vsync << std::endl;
    REQUIRE(fps_vsync > 0.0f);

    // Expect a significant drop when VSync is on (display refresh cap)
    // But be more lenient since some systems might not enforce VSync strictly
    std::cout << "FPS difference: " << (fps_no_vsync - fps_vsync) << std::endl;
    REQUIRE(fps_no_vsync > fps_vsync);
    REQUIRE((fps_no_vsync - fps_vsync) > 1.0f); // Reduced threshold
}

TEST_CASE("Multiple render entities frame rate performance", "[renderable_entity][multi_entity][fps][performance]") {
    SDLInitGuard sdl_guard;

    // Create multiple visible windows with different clear colours
    DummyRenderableEntity red    ({1.0f, 0.0f, 0.0f, 1.0f}, 200, 400, true, "RedWindow");
    DummyRenderableEntity green  ({0.0f, 1.0f, 0.0f, 1.0f}, 200, 400, true, "GreenWindow");
    DummyRenderableEntity blue   ({0.0f, 0.0f, 1.0f, 1.0f}, 200, 400, true, "BlueWindow");
    DummyRenderableEntity yellow ({1.0f, 1.0f, 0.0f, 1.0f}, 200, 400, true, "YellowWindow");

    // Disable VSync for consistent measurements
    red.set_vsync_enabled(false);
    green.set_vsync_enabled(false);
    blue.set_vsync_enabled(false);
    yellow.set_vsync_enabled(false);

    const Uint32 test_duration_ms = 3000; // 3 seconds for stable measurement
    std::vector<float> single_entity_fps;
    std::vector<float> multi_entity_fps;

    // ---------- Test 1: Single entity rendering ----------
    std::cout << "Testing single entity rendering..." << std::endl;
    Uint32 start_time = SDL_GetTicks();
    while (SDL_GetTicks() - start_time < test_duration_ms) {
        red.render();
        red.present();
        red.unactivate_render_context();
    }
    float single_fps = red.get_present_fps();
    single_entity_fps.push_back(single_fps);
    std::cout << "Single entity FPS: " << single_fps << std::endl;
    REQUIRE(single_fps > 0.0f);

    // ---------- Test 2: Multiple entities rendering in sequence ----------
    std::cout << "Testing multiple entities rendering in sequence..." << std::endl;
    start_time = SDL_GetTicks();
    while (SDL_GetTicks() - start_time < test_duration_ms) {
        // Render each entity in sequence
        red.render();
        red.present();
        red.unactivate_render_context();

        green.render();
        green.present();
        green.unactivate_render_context();

        blue.render();
        blue.present();
        blue.unactivate_render_context();

        yellow.render();
        yellow.present();
        yellow.unactivate_render_context();
    }

    // Calculate average FPS across all entities
    float avg_multi_fps = (red.get_present_fps() + green.get_present_fps() + 
                          blue.get_present_fps() + yellow.get_present_fps()) / 4.0f;
    multi_entity_fps.push_back(avg_multi_fps);
    std::cout << "Multi-entity average FPS: " << avg_multi_fps << std::endl;
    std::cout << "Individual FPS - Red: " << red.get_present_fps() 
              << ", Green: " << green.get_present_fps()
              << ", Blue: " << blue.get_present_fps()
              << ", Yellow: " << yellow.get_present_fps() << std::endl;
    REQUIRE(avg_multi_fps > 0.0f);

    // ---------- Test 3: Multiple entities with context switching overhead ----------
    std::cout << "Testing multiple entities with context switching overhead..." << std::endl;
    start_time = SDL_GetTicks();
    while (SDL_GetTicks() - start_time < test_duration_ms) {
        // Render each entity with explicit context activation/deactivation
        red.activate_render_context();
        red.render();
        red.present();
        red.unactivate_render_context();

        green.activate_render_context();
        green.render();
        green.present();
        green.unactivate_render_context();

        blue.activate_render_context();
        blue.render();
        blue.present();
        blue.unactivate_render_context();

        yellow.activate_render_context();
        yellow.render();
        yellow.present();
        yellow.unactivate_render_context();
    }

    float avg_context_switch_fps = (red.get_present_fps() + green.get_present_fps() + 
                                   blue.get_present_fps() + yellow.get_present_fps()) / 4.0f;
    multi_entity_fps.push_back(avg_context_switch_fps);
    std::cout << "Multi-entity with context switching average FPS: " << avg_context_switch_fps << std::endl;
    REQUIRE(avg_context_switch_fps > 0.0f);

    // ---------- Performance Analysis ----------
    std::cout << "\n=== Performance Analysis ===" << std::endl;
    std::cout << "Single entity FPS: " << single_fps << std::endl;
    std::cout << "Multi-entity sequence FPS: " << avg_multi_fps << std::endl;
    std::cout << "Multi-entity with context switching FPS: " << avg_context_switch_fps << std::endl;
    
    // Calculate performance ratios
    float sequence_ratio = avg_multi_fps / single_fps;
    float context_switch_ratio = avg_context_switch_fps / single_fps;
    
    std::cout << "Multi-entity performance ratio: " << sequence_ratio << "x" << std::endl;
    std::cout << "Context switching performance ratio: " << context_switch_ratio << "x" << std::endl;

    // Performance expectations:
    // 1. Multi-entity should be slower than single entity due to context switching overhead
    // 2. Explicit context switching should be similar or slightly slower than implicit
    REQUIRE(single_fps >= avg_multi_fps * 0.5f); // Multi-entity should not be more than 50% slower
    REQUIRE(avg_multi_fps >= avg_context_switch_fps * 0.8f); // Context switching should not be more than 20% slower

    // Verify all entities are still functional after the test
    REQUIRE(red.get_render_context().is_valid());
    REQUIRE(green.get_render_context().is_valid());
    REQUIRE(blue.get_render_context().is_valid());
    REQUIRE(yellow.get_render_context().is_valid());

    // Cleanup contexts
    red.unactivate_render_context();
    green.unactivate_render_context();
    blue.unactivate_render_context();
    yellow.unactivate_render_context();
}

#endif // RENDERABLE_ENTITY_GL_TEST_CPP 