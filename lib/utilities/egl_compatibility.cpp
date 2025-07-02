#include "utilities/egl_compatibility.h"
#include <unordered_map>
#include <EGL/eglext.h>
#include <cstring> // for strstr

// Static member initialization
EGLDisplay EGLCompatibility::s_eglDisplay = EGL_NO_DISPLAY;
EGLConfig  EGLCompatibility::s_eglConfig  = nullptr;
bool       EGLCompatibility::s_initialized = false;
// New: remember which client version we ended up with (2 or 3)
static int s_contextClientVersion = 0;
static bool s_deviceDisplayActive = false; // true if we are currently using a platform DEVICE display

std::unordered_map<SDL_Window*, EGLSurface> EGLCompatibility::s_surfaces;
std::unordered_map<SDL_Window*, EGLContext> EGLCompatibility::s_contexts;
std::unordered_map<SDL_Window*, int> EGLCompatibility::s_surfaceIntervals;

// Helper to check for extension support in a space separated list
static bool contains_extension(const char* ext_list, const char* ext) {
    if (!ext_list || !ext) return false;
    const char* start = ext_list;
    size_t len = std::strlen(ext);
    while (true) {
        const char* where = std::strstr(start, ext);
        if (!where) return false;
        const char* terminator = where + len;
        if ((where == start || *(where - 1) == ' ') && (*terminator == ' ' || *terminator == '\0'))
            return true; // Found as whole token
        start = terminator;
    }
}

// Attempt to obtain an EGLDisplay corresponding to a *hardware* device using
// EGL_EXT_device_enumeration / EGL_EXT_device_query. Returns EGL_NO_DISPLAY if
// the extensions are not present or no suitable device was found.
static EGLDisplay choose_hardware_display() {
#ifdef EGL_EXT_platform_base
    auto eglQueryDevicesEXT = reinterpret_cast<PFNEGLQUERYDEVICESEXTPROC>(eglGetProcAddress("eglQueryDevicesEXT"));
    auto eglGetPlatformDisplayEXT = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    auto eglQueryDeviceStringEXT = reinterpret_cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(eglGetProcAddress("eglQueryDeviceStringEXT"));
    if (!eglQueryDevicesEXT || !eglGetPlatformDisplayEXT || !eglQueryDeviceStringEXT) {
        return EGL_NO_DISPLAY;
    }

    EGLDeviceEXT devices[8] = {0};
    EGLint numDevices = 0;
    if (!eglQueryDevicesEXT(8, devices, &numDevices) || numDevices == 0) {
        return EGL_NO_DISPLAY;
    }

    for (int i = 0; i < numDevices; ++i) {
        const char* devExt = eglQueryDeviceStringEXT(devices[i], EGL_EXTENSIONS);
        // Skip devices that explicitly advertise the MESA software renderer
        if (contains_extension(devExt, "EGL_MESA_device_software")) {
            continue;
        }
        // Prefer this device
        EGLDisplay dpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, devices[i], nullptr);
        if (dpy != EGL_NO_DISPLAY) {
            return dpy;
        }
    }
#endif
    return EGL_NO_DISPLAY;
}

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

    // Now that a context is current, print the GL renderer string once.
    static bool printed_renderer = false;
    if (!printed_renderer) {
        const char* rendererStr = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        std::cout << "EGL: GL Renderer: " << (rendererStr ? rendererStr : "Unknown") << std::endl;
        printed_renderer = true;
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

        // Reapply stored swap interval for this surface, if any.
        if (auto it = s_surfaceIntervals.find(window); it != s_surfaceIntervals.end()) {
            eglSwapInterval(s_eglDisplay, it->second);
        }
    }
}

void EGLCompatibility::set_swap_interval(SDL_Window* window, int interval) {
    if (!window) return;
    if (s_eglDisplay == EGL_NO_DISPLAY) return;

    // Update map
    s_surfaceIntervals[window] = interval;

    // Apply immediately if this window is current
    SDL_Window* currentWin = SDL_GL_GetCurrentWindow();
    if (currentWin == window) {
        eglSwapInterval(s_eglDisplay, interval);
    }
}

bool EGLCompatibility::initialize_egl_display() {
    // NEW: Make sure we are requesting the GLES API up-front. If this call
    // fails we fall back to the default behaviour but print a warning so the
    // user understands why the context might be slow.
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        std::cerr << "EGL: Failed to bind OpenGL ES API, falling back to default" << std::endl;
    }

    // Try to pick a hardware device first (skips llvmpipe / software nodes)
    s_eglDisplay = choose_hardware_display();
    if (s_eglDisplay != EGL_NO_DISPLAY) {
        s_deviceDisplayActive = true;
    } else {
        // Fallback to the default display
        s_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        s_deviceDisplayActive = false;
    }

    if (s_eglDisplay == EGL_NO_DISPLAY) {
        std::cerr << "EGL: Failed to get EGL display" << std::endl;
        return false;
    }

    EGLint majorVersion, minorVersion;
    if (!eglInitialize(s_eglDisplay, &majorVersion, &minorVersion)) {
        std::cerr << "EGL: Failed to initialize EGL" << std::endl;
        return false;
    }

    std::cout << "EGL: Using display with version " << majorVersion << "." << minorVersion << std::endl;
    std::cout << "EGL: Vendor  : " << eglQueryString(s_eglDisplay, EGL_VENDOR) << std::endl;
    // Note: Can't query GL_RENDERER until a context is current. The actual
    // renderer string is printed later once the first context is created.

#ifdef EGL_EXT_device_query
    // Print the underlying device node if the extension is present
    auto eglQueryDisplayAttribEXT = reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(eglGetProcAddress("eglQueryDisplayAttribEXT"));
    auto eglQueryDeviceStringEXT  = reinterpret_cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(eglGetProcAddress("eglQueryDeviceStringEXT"));
    if (eglQueryDisplayAttribEXT && eglQueryDeviceStringEXT) {
        EGLAttrib devAttr = 0;
        if (eglQueryDisplayAttribEXT(s_eglDisplay, EGL_DEVICE_EXT, &devAttr)) {
            EGLDeviceEXT dev = reinterpret_cast<EGLDeviceEXT>(devAttr);
            const char* devPath = eglQueryDeviceStringEXT(dev, EGL_DRM_DEVICE_FILE_EXT);
            if (!devPath) {
                // Some drivers expose only the render node
                devPath = eglQueryDeviceStringEXT(dev, EGL_DRM_RENDER_NODE_FILE_EXT);
            }
            std::cout << "EGL: Device  : " << (devPath ? devPath : "Unknown") << std::endl;
        }
    }
#endif
    return true;
}

bool EGLCompatibility::choose_egl_config() {
    // Try ES 3 first, but gracefully fall back to ES 2 if that is the only
    // accelerated option on this platform (e.g. Raspberry Pi 3 / older Mesa).
    const struct VersionTry {
        EGLint renderable_bit;
        int     client_version; // value for EGL_CONTEXT_CLIENT_VERSION later
    } tries[] = {
        {EGL_OPENGL_ES3_BIT, 3},
        {EGL_OPENGL_ES2_BIT, 2},
    };

    EGLint numConfigs = 0;
    for (const auto & t : tries) {
        const EGLint configAttribs[] = {
            // Window surface capable
            EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
            // Accelerated GLES config for the requested version
            EGL_RENDERABLE_TYPE, t.renderable_bit,
            // 8-8-8-8 RGBA + 24-bit depth + 8-bit stencil like before
            EGL_RED_SIZE,        8,
            EGL_GREEN_SIZE,      8,
            EGL_BLUE_SIZE,       8,
            EGL_ALPHA_SIZE,      8,
            EGL_DEPTH_SIZE,      24,
            EGL_STENCIL_SIZE,    8,
            EGL_NONE
        };

        if (eglChooseConfig(s_eglDisplay, configAttribs, &s_eglConfig, 1, &numConfigs) && numConfigs > 0) {
            // Remember which client version we will request later
            s_contextClientVersion = t.client_version;
            return true; // success with full attributes
        }

        // Fallback: try with only the required attributes (no depth/stencil/alpha constraints)
        const EGLint minimalAttribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, t.renderable_bit,
            EGL_NONE
        };

        if (eglChooseConfig(s_eglDisplay, minimalAttribs, &s_eglConfig, 1, &numConfigs) && numConfigs > 0) {
            std::cerr << "EGL: Using minimal framebuffer attributes (depth/stencil may be emulated)" << std::endl;
            s_contextClientVersion = t.client_version;
            return true;
        }
    }

    // If we were on a DEVICE display, retry with the default X11/DRM display once.
    if (s_deviceDisplayActive) {
        std::cerr << "EGL: No window configs on DEVICE display, retrying default display" << std::endl;
        eglTerminate(s_eglDisplay);
        s_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (s_eglDisplay != EGL_NO_DISPLAY && eglInitialize(s_eglDisplay, nullptr, nullptr)) {
            s_deviceDisplayActive = false;
            // Re-run the entire selection recursively once
            return choose_egl_config();
        }
    }

    std::cerr << "EGL: Failed to find a suitable ES2/ES3 accelerated config" << std::endl;
    return false;
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

EGLContext EGLCompatibility::create_egl_context(EGLSurface /*surface*/) {
    // Use the version that was picked in choose_egl_config(). Default to 2 if
    // something went wrong just to be safe.
    if (s_contextClientVersion == 0) {
        s_contextClientVersion = 2;
    }

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, s_contextClientVersion,
        EGL_NONE
    };

    EGLContext ctx = eglCreateContext(s_eglDisplay, s_eglConfig, EGL_NO_CONTEXT, contextAttribs);
    if (ctx == EGL_NO_CONTEXT) {
        std::cerr << "EGL: Failed to create EGL context (" << s_contextClientVersion << ")" << std::endl;
    }

    return ctx;
} 