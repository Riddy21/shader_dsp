#include "graphics_core/egl_compatibility.h"

// Static member initialization
EGLDisplay EGLCompatibility::s_eglDisplay = EGL_NO_DISPLAY;
EGLSurface EGLCompatibility::s_eglSurface = EGL_NO_SURFACE;
EGLContext EGLCompatibility::s_eglContext = EGL_NO_CONTEXT;
EGLConfig EGLCompatibility::s_eglConfig = nullptr;
bool EGLCompatibility::s_initialized = false;

bool EGLCompatibility::initialize_egl_context(SDL_Window* window, SDL_GLContext& out_context) {
    if (!window) {
        std::cerr << "EGL: Invalid window pointer" << std::endl;
        return false;
    }

    // Initialize EGL display if not already done
    if (!s_initialized) {
        if (!initialize_egl_display()) {
            return false;
        }
        if (!choose_egl_config()) {
            return false;
        }
        s_initialized = true;
    }

    // Create EGL surface
    if (!create_egl_surface(window)) {
        return false;
    }

    // Create EGL context
    if (!create_egl_context()) {
        return false;
    }

    // Make current
    if (!eglMakeCurrent(s_eglDisplay, s_eglSurface, s_eglSurface, s_eglContext)) {
        std::cerr << "EGL: Failed to make context current" << std::endl;
        return false;
    }

    // Set the output context (we'll use a dummy value since we're using EGL)
    out_context = (SDL_GLContext)0x1; // Dummy value, we don't actually use SDL's GL context

    std::cout << "EGL: OpenGL ES context initialized successfully" << std::endl;
    return true;
}

void EGLCompatibility::cleanup_egl_context(SDL_GLContext context) {
    if (s_eglContext != EGL_NO_CONTEXT) {
        eglDestroyContext(s_eglDisplay, s_eglContext);
        s_eglContext = EGL_NO_CONTEXT;
    }
    
    if (s_eglSurface != EGL_NO_SURFACE) {
        eglDestroySurface(s_eglDisplay, s_eglSurface);
        s_eglSurface = EGL_NO_SURFACE;
    }
    
    if (s_eglDisplay != EGL_NO_DISPLAY) {
        eglTerminate(s_eglDisplay);
        s_eglDisplay = EGL_NO_DISPLAY;
    }
    
    s_initialized = false;
}

void EGLCompatibility::swap_buffers(SDL_Window* window) {
    if (s_eglDisplay != EGL_NO_DISPLAY && s_eglSurface != EGL_NO_SURFACE) {
        eglSwapBuffers(s_eglDisplay, s_eglSurface);
    }
}

void EGLCompatibility::make_current(SDL_Window* window, SDL_GLContext context) {
    if (s_eglDisplay != EGL_NO_DISPLAY && s_eglSurface != EGL_NO_SURFACE && s_eglContext != EGL_NO_CONTEXT) {
        eglMakeCurrent(s_eglDisplay, s_eglSurface, s_eglSurface, s_eglContext);
    }
}

bool EGLCompatibility::initialize_egl_display() {
    s_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (s_eglDisplay == EGL_NO_DISPLAY) {
        std::cerr << "EGL: Failed to get EGL display" << std::endl;
        return false;
    }

    EGLint majorVersion, minorVersion;
    if (!eglInitialize(s_eglDisplay, &majorVersion, &minorVersion)) {
        std::cerr << "EGL: Failed to initialize EGL" << std::endl;
        return false;
    }

    std::cout << "EGL: Version " << majorVersion << "." << minorVersion << std::endl;
    return true;
}

bool EGLCompatibility::choose_egl_config() {
    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };

    EGLint numConfigs;
    if (!eglChooseConfig(s_eglDisplay, configAttribs, &s_eglConfig, 1, &numConfigs)) {
        std::cerr << "EGL: Failed to choose EGL config" << std::endl;
        return false;
    }

    if (numConfigs == 0) {
        std::cerr << "EGL: No suitable EGL config found" << std::endl;
        return false;
    }

    return true;
}

bool EGLCompatibility::create_egl_surface(SDL_Window* window) {
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
        std::cerr << "EGL: Failed to get window WM info" << std::endl;
        return false;
    }

    s_eglSurface = eglCreateWindowSurface(s_eglDisplay, s_eglConfig, 
                                        (EGLNativeWindowType)wmInfo.info.x11.window, NULL);
    if (s_eglSurface == EGL_NO_SURFACE) {
        std::cerr << "EGL: Failed to create EGL surface" << std::endl;
        return false;
    }

    return true;
}

bool EGLCompatibility::create_egl_context() {
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    s_eglContext = eglCreateContext(s_eglDisplay, s_eglConfig, EGL_NO_CONTEXT, contextAttribs);
    if (s_eglContext == EGL_NO_CONTEXT) {
        std::cerr << "EGL: Failed to create EGL context" << std::endl;
        return false;
    }

    return true;
} 