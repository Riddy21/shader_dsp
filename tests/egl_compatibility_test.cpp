#include "catch2/catch_all.hpp"
#include "test_sdl_manager.h"
#include "utilities/egl_compatibility.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <vector>

TEST_CASE("EGLCompatibility stress test - repeated init and cleanup", "[egl_compatibility]") {
    TestSDLGuard sdl_guard(SDL_INIT_VIDEO);

    const int max_attempts = 100;
    int success_count = 0;

    for (int i = 0; i < max_attempts; ++i) {
        SDL_Window* window = SDL_CreateWindow(
            ("Test Window " + std::to_string(i)).c_str(),
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            1, 1, SDL_WINDOW_HIDDEN
        );
        REQUIRE(window != nullptr);

        SDL_GLContext dummy_ctx;
        bool success = EGLCompatibility::initialize_egl_context(window, dummy_ctx);

        if (!success) {
            std::cerr << "Failed after " << success_count << " successful init/cleanup cycles" << std::endl;
            SDL_DestroyWindow(window);
            break;
        }

        // Cleanup immediately
        EGLCompatibility::cleanup_egl_context(window);
        SDL_DestroyWindow(window);

        success_count++;
    }

    REQUIRE(success_count > 10);
    REQUIRE(success_count < max_attempts);
}
