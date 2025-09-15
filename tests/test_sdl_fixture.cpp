#include "test_sdl_manager.h"
#include "catch2/catch_all.hpp"

/**
 * Global test fixture to ensure SDL is properly cleaned up after all tests.
 * This is registered with Catch2 to run once at the end of the test suite.
 */
class SDLTestFixture {
public:
    ~SDLTestFixture() {
        // Ensure SDL is properly quit after all tests
        TestSDLManager::get_instance().quit();
    }
};

// Register the fixture with Catch2
CATCH_REGISTER_FIXTURE(SDLTestFixture);
