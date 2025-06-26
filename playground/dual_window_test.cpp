#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <unistd.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>

// EGL globals
EGLDisplay eglDisplay = EGL_NO_DISPLAY;
EGLConfig eglConfig = nullptr;
EGLContext eglContext = EGL_NO_CONTEXT;
EGLSurface eglSurface1 = EGL_NO_SURFACE;
EGLSurface eglSurface2 = EGL_NO_SURFACE;

bool initialize_egl_display() {
    eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL_NO_DISPLAY) {
        std::cerr << "EGL: Failed to get EGL display" << std::endl;
        return false;
    }

    EGLint majorVersion, minorVersion;
    if (!eglInitialize(eglDisplay, &majorVersion, &minorVersion)) {
        std::cerr << "EGL: Failed to initialize EGL" << std::endl;
        return false;
    }

    std::cout << "EGL: Version " << majorVersion << "." << minorVersion << std::endl;
    return true;
}

bool choose_egl_config() {
    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE
    };

    EGLint numConfigs;
    if (!eglChooseConfig(eglDisplay, configAttribs, &eglConfig, 1, &numConfigs)) {
        std::cerr << "EGL: Failed to choose EGL config" << std::endl;
        return false;
    }

    if (numConfigs == 0) {
        std::cerr << "EGL: No suitable EGL config found" << std::endl;
        return false;
    }

    return true;
}

bool create_egl_context() {
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    eglContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttribs);
    if (eglContext == EGL_NO_CONTEXT) {
        std::cerr << "EGL: Failed to create EGL context" << std::endl;
        return false;
    }

    return true;
}

bool create_egl_surface(SDL_Window* window, EGLSurface& surface) {
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
        std::cerr << "EGL: Failed to get window WM info: " << SDL_GetError() << std::endl;
        return false;
    }

    surface = eglCreateWindowSurface(eglDisplay, eglConfig, 
                                   (EGLNativeWindowType)wmInfo.info.x11.window, NULL);
    if (surface == EGL_NO_SURFACE) {
        std::cerr << "EGL: Failed to create EGL surface" << std::endl;
        return false;
    }

    return true;
}

void cleanup_egl() {
    if (eglContext != EGL_NO_CONTEXT) {
        eglDestroyContext(eglDisplay, eglContext);
        eglContext = EGL_NO_CONTEXT;
    }
    
    if (eglSurface1 != EGL_NO_SURFACE) {
        eglDestroySurface(eglDisplay, eglSurface1);
        eglSurface1 = EGL_NO_SURFACE;
    }
    
    if (eglSurface2 != EGL_NO_SURFACE) {
        eglDestroySurface(eglDisplay, eglSurface2);
        eglSurface2 = EGL_NO_SURFACE;
    }
    
    if (eglDisplay != EGL_NO_DISPLAY) {
        eglTerminate(eglDisplay);
        eglDisplay = EGL_NO_DISPLAY;
    }
}

int main() {
    std::cout << "=== Dual SDL Window Test with EGL ===" << std::endl;
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL2: " << SDL_GetError() << std::endl;
        return -1;
    }
    
    std::cout << "✓ SDL initialized successfully" << std::endl;
    
    // Initialize EGL display
    if (!initialize_egl_display()) {
        SDL_Quit();
        return -1;
    }
    
    std::cout << "✓ EGL display initialized" << std::endl;
    
    // Choose EGL config
    if (!choose_egl_config()) {
        cleanup_egl();
        SDL_Quit();
        return -1;
    }
    
    std::cout << "✓ EGL config chosen" << std::endl;
    
    // Create EGL context
    if (!create_egl_context()) {
        cleanup_egl();
        SDL_Quit();
        return -1;
    }
    
    std::cout << "✓ EGL context created" << std::endl;
    
    // Create first window (no SDL_WINDOW_OPENGL flag since we're using EGL)
    SDL_Window* window1 = SDL_CreateWindow(
        "Test Window 1 (EGL)",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        800, 600,
        SDL_WINDOW_SHOWN
    );
    
    if (!window1) {
        std::cerr << "Failed to create first window: " << SDL_GetError() << std::endl;
        cleanup_egl();
        SDL_Quit();
        return -1;
    }
    
    std::cout << "✓ First window created successfully (800x600)" << std::endl;
    
    // Create EGL surface for first window
    if (!create_egl_surface(window1, eglSurface1)) {
        SDL_DestroyWindow(window1);
        cleanup_egl();
        SDL_Quit();
        return -1;
    }
    
    std::cout << "✓ EGL surface created for first window" << std::endl;
    
    // Create second window
    SDL_Window* window2 = SDL_CreateWindow(
        "Test Window 2 (EGL)",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        400, 200,
        SDL_WINDOW_SHOWN
    );
    
    if (!window2) {
        std::cerr << "Failed to create second window: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window1);
        cleanup_egl();
        SDL_Quit();
        return -1;
    }
    
    std::cout << "✓ Second window created successfully (400x200)" << std::endl;
    
    // Create EGL surface for second window
    if (!create_egl_surface(window2, eglSurface2)) {
        SDL_DestroyWindow(window2);
        SDL_DestroyWindow(window1);
        cleanup_egl();
        SDL_Quit();
        return -1;
    }
    
    std::cout << "✓ EGL surface created for second window" << std::endl;
    
    // Test making contexts current
    if (!eglMakeCurrent(eglDisplay, eglSurface1, eglSurface1, eglContext)) {
        std::cerr << "Failed to make first context current" << std::endl;
    } else {
        std::cout << "✓ First context made current" << std::endl;
    }
    
    if (!eglMakeCurrent(eglDisplay, eglSurface2, eglSurface2, eglContext)) {
        std::cerr << "Failed to make second context current" << std::endl;
    } else {
        std::cout << "✓ Second context made current" << std::endl;
    }
    
    // Print window information
    int w1, h1, w2, h2;
    SDL_GetWindowSize(window1, &w1, &h1);
    SDL_GetWindowSize(window2, &w2, &h2);
    
    std::cout << std::endl;
    std::cout << "Window 1: " << w1 << "x" << h1 << std::endl;
    std::cout << "Window 2: " << w2 << "x" << h2 << std::endl;
    
    // Check if both windows are visible
    Uint32 flags1 = SDL_GetWindowFlags(window1);
    Uint32 flags2 = SDL_GetWindowFlags(window2);
    
    std::cout << std::endl;
    std::cout << "Window 1 flags: 0x" << std::hex << flags1 << std::dec;
    if (flags1 & SDL_WINDOW_SHOWN) std::cout << " (SHOWN)";
    if (flags1 & SDL_WINDOW_HIDDEN) std::cout << " (HIDDEN)";
    if (flags1 & SDL_WINDOW_MINIMIZED) std::cout << " (MINIMIZED)";
    std::cout << std::endl;
    
    std::cout << "Window 2 flags: 0x" << std::hex << flags2 << std::dec;
    if (flags2 & SDL_WINDOW_SHOWN) std::cout << " (SHOWN)";
    if (flags2 & SDL_WINDOW_HIDDEN) std::cout << " (HIDDEN)";
    if (flags2 & SDL_WINDOW_MINIMIZED) std::cout << " (MINIMIZED)";
    std::cout << std::endl;
    
    std::cout << std::endl;
    std::cout << "Both windows should now be visible on screen." << std::endl;
    std::cout << "Test will run for 10 seconds, then clean up..." << std::endl;
    
    // Print OpenGL info
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version = glGetString(GL_VERSION);
    std::cout << "OpenGL Renderer: " << renderer << std::endl;
    std::cout << "OpenGL Version: " << version << std::endl;
    
    // Run a simple event loop for 10 seconds
    Uint32 start_time = SDL_GetTicks();
    bool running = true;
    SDL_Event event;
    
    while (running && (SDL_GetTicks() - start_time) < 10000) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                std::cout << "Quit event received" << std::endl;
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_q) {
                    std::cout << "Q key pressed - exiting" << std::endl;
                    running = false;
                }
            }
        }
        
        // Clear first window with blue color
        eglMakeCurrent(eglDisplay, eglSurface1, eglSurface1, eglContext);
        glClearColor(0.2f, 0.3f, 0.8f, 1.0f); // Blue
        glClear(GL_COLOR_BUFFER_BIT);
        eglSwapBuffers(eglDisplay, eglSurface1);
        
        // Clear second window with red color
        eglMakeCurrent(eglDisplay, eglSurface2, eglSurface2, eglContext);
        glClearColor(0.8f, 0.3f, 0.2f, 1.0f); // Red
        glClear(GL_COLOR_BUFFER_BIT);
        eglSwapBuffers(eglDisplay, eglSurface2);
        
        SDL_Delay(16); // ~60 FPS
    }
    
    std::cout << std::endl;
    std::cout << "Cleaning up..." << std::endl;
    
    // Cleanup
    SDL_DestroyWindow(window2);
    SDL_DestroyWindow(window1);
    cleanup_egl();
    SDL_Quit();
    
    std::cout << "✓ Test completed successfully!" << std::endl;
    std::cout << std::endl;
    std::cout << "Results:" << std::endl;
    std::cout << "- If you saw both windows (one blue, one red), multiple EGL windows work!" << std::endl;
    std::cout << "- If you only saw one window, there may be a Raspberry Pi specific issue." << std::endl;
    
    return 0;
} 