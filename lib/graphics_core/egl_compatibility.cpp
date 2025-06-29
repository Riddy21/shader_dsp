#include "graphics_core/egl_compatibility.h"
#include <unordered_map>

// Static member initialization
EGLDisplay EGLCompatibility::s_eglDisplay = EGL_NO_DISPLAY;
EGLConfig  EGLCompatibility::s_eglConfig  = nullptr;
bool       EGLCompatibility::s_initialized = false;

std::unordered_map<SDL_Window*, EGLSurface> EGLCompatibility::s_surfaces;
std::unordered_map<SDL_Window*, EGLContext> EGLCompatibility::s_contexts;

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

    // Create EGL surface for this window (or fetch existing)
    EGLSurface surface = EGL_NO_SURFACE;
    if (auto it = s_surfaces.find(window); it != s_surfaces.end()) {
        surface = it->second;
    } else {
        surface = create_egl_surface(window);
        if (surface == EGL_NO_SURFACE) {
            return false;
        }
        s_surfaces[window] = surface;
    }

    // Create EGL context for this window (or fetch existing)
    EGLContext context = EGL_NO_CONTEXT;
    if (auto it = s_contexts.find(window); it != s_contexts.end()) {
        context = it->second;
    } else {
        context = create_egl_context(surface);
        if (context == EGL_NO_CONTEXT) {
            return false;
        }
        s_contexts[window] = context;
    }

    // Make current
    if (!eglMakeCurrent(s_eglDisplay, surface, surface, context)) {
        std::cerr << "EGL: Failed to make context current" << std::endl;
        return false;
    }

    // Set the output context (dummy value for SDL)
    out_context = (SDL_GLContext)0x1;

    std::cout << "EGL: OpenGL ES context initialized successfully" << std::endl;
    return true;
}

void EGLCompatibility::cleanup_egl_context(SDL_Window* window) {
    if (!window) return;

    if (auto sit = s_surfaces.find(window); sit != s_surfaces.end()) {
        eglDestroySurface(s_eglDisplay, sit->second);
        s_surfaces.erase(sit);
    }

    if (auto cit = s_contexts.find(window); cit != s_contexts.end()) {
        eglDestroyContext(s_eglDisplay, cit->second);
        s_contexts.erase(cit);
    }

    // If no more surfaces, teardown EGL display completely
    if (s_surfaces.empty()) {
        for (auto& [win, ctx] : s_contexts) {
            eglDestroyContext(s_eglDisplay, ctx);
        }
        s_contexts.clear();

        if (s_eglDisplay != EGL_NO_DISPLAY) {
            eglTerminate(s_eglDisplay);
            s_eglDisplay = EGL_NO_DISPLAY;
        }
        s_initialized = false;
    }
}

void EGLCompatibility::swap_buffers(SDL_Window* window) {
    if (!window) return;
    if (s_eglDisplay == EGL_NO_DISPLAY) return;
    auto it = s_surfaces.find(window);
    if (it != s_surfaces.end()) {
        eglSwapBuffers(s_eglDisplay, it->second);
    }
}

void EGLCompatibility::make_current(SDL_Window* window, SDL_GLContext context) {
    if (!window) return;
    if (s_eglDisplay == EGL_NO_DISPLAY) return;
    auto sit = s_surfaces.find(window);
    auto cit = s_contexts.find(window);
    if (sit != s_surfaces.end() && cit != s_contexts.end()) {
        eglMakeCurrent(s_eglDisplay, sit->second, sit->second, cit->second);
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

EGLSurface EGLCompatibility::create_egl_surface(SDL_Window* window) {
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
        std::cerr << "EGL: Failed to get window WM info" << std::endl;
        return EGL_NO_SURFACE;
    }

    EGLSurface surface = eglCreateWindowSurface(s_eglDisplay, s_eglConfig,
                                                (EGLNativeWindowType)wmInfo.info.x11.window, NULL);
    if (surface == EGL_NO_SURFACE) {
        std::cerr << "EGL: Failed to create EGL surface" << std::endl;
    }
    return surface;
}

EGLContext EGLCompatibility::create_egl_context(EGLSurface surface) {
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    EGLContext ctx = eglCreateContext(s_eglDisplay, s_eglConfig, EGL_NO_CONTEXT, contextAttribs);
    if (ctx == EGL_NO_CONTEXT) {
        std::cerr << "EGL: Failed to create EGL context" << std::endl;
    }

    return ctx;
} 