#ifndef TEST_SDL_MANAGER_H
#define TEST_SDL_MANAGER_H

#include <SDL2/SDL.h>
#include <mutex>
#include <atomic>

#include "utilities/egl_compatibility.h"

/**
 * Centralized SDL manager for tests to avoid SDL initialization conflicts.
 * This singleton ensures SDL is initialized once and only quit at the end of all tests.
 */
class TestSDLManager {
public:
    static TestSDLManager& get_instance() {
        static TestSDLManager instance;
        return instance;
    }

    /**
     * Initialize SDL if not already initialized.
     * Thread-safe and can be called multiple times.
     * @param flags SDL initialization flags (default: SDL_INIT_VIDEO)
     * @return true if SDL was successfully initialized or was already initialized
     */
    bool initialize(Uint32 flags = SDL_INIT_VIDEO) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_initialized.load()) {
            return true; // Already initialized
        }

        if (SDL_Init(flags) != 0) {
            return false; // Failed to initialize
        }

        m_initialized.store(true);
        return true;
    }

    /**
     * Initialize SDL subsystem if not already initialized.
     * Thread-safe and can be called multiple times.
     * @param flags SDL subsystem flags
     * @return true if subsystem was successfully initialized or was already initialized
     */
    bool initialize_subsystem(Uint32 flags) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (SDL_WasInit(flags) != 0) {
            return true; // Already initialized
        }

        if (SDL_InitSubSystem(flags) != 0) {
            return false; // Failed to initialize
        }

        return true;
    }

    /**
     * Check if SDL is initialized.
     * @return true if SDL is initialized
     */
    bool is_initialized() const {
        return m_initialized.load();
    }

    /**
     * Quit SDL. This should only be called once at the end of all tests.
     * Thread-safe.
     */
    void quit() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_initialized.load()) {
            SDL_Quit();
            m_initialized.store(false);
        }
        EGLCompatibility::global_cleanup();
    }

    /**
     * Quit SDL subsystem.
     * @param flags SDL subsystem flags to quit
     */
    void quit_subsystem(Uint32 flags) {
        std::lock_guard<std::mutex> lock(m_mutex);
        SDL_QuitSubSystem(flags);
    }

    // Prevent copying
    TestSDLManager(const TestSDLManager&) = delete;
    TestSDLManager& operator=(const TestSDLManager&) = delete;

private:
    TestSDLManager() = default;
    ~TestSDLManager() {
        quit();
        EGLCompatibility::global_cleanup();
    }

    std::atomic<bool> m_initialized{false};
    std::mutex m_mutex;
};

/**
 * RAII helper for tests that need SDL initialization.
 * This replaces the individual SDLInitGuard classes in each test file.
 */
class TestSDLGuard {
public:
    /**
     * Initialize SDL with the specified flags.
     * @param flags SDL initialization flags (default: SDL_INIT_VIDEO)
     */
    explicit TestSDLGuard(Uint32 flags = SDL_INIT_VIDEO) 
        : m_flags(flags), m_we_initialized(false) {
        if (!TestSDLManager::get_instance().is_initialized()) {
            if (TestSDLManager::get_instance().initialize(flags)) {
                m_we_initialized = true;
            }
        }
    }

    /**
     * Initialize SDL subsystem with the specified flags.
     * @param flags SDL subsystem flags
     */
    explicit TestSDLGuard(Uint32 flags, bool /* subsystem */) 
        : m_flags(flags), m_we_initialized(false) {
        if (SDL_WasInit(flags) == 0) {
            if (TestSDLManager::get_instance().initialize_subsystem(flags)) {
                m_we_initialized = true;
            }
        }
    }

    /**
     * Destructor - does NOT quit SDL, as it's managed centrally.
     */
    ~TestSDLGuard() {
        // Do nothing - SDL cleanup is handled centrally by TestSDLManager
    }

    /**
     * Check if SDL was successfully initialized by this guard.
     * @return true if SDL was initialized
     */
    bool is_initialized() const {
        return TestSDLManager::get_instance().is_initialized();
    }

private:
    Uint32 m_flags;
    bool m_we_initialized;
};

#endif // TEST_SDL_MANAGER_H
