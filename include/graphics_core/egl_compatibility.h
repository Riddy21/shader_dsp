#pragma once
#ifndef EGL_COMPATIBILITY_H
#define EGL_COMPATIBILITY_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

// EGL compatibility layer for OpenGL ES context creation
class EGLCompatibility {
public:
    // Initializes an EGL context and surface for the given SDL_Window*.
    // The created context is returned via out_context, but note that SDL only
    // sees a dummy value (we drive EGL directly).
    static bool initialize_egl_context(SDL_Window* window, SDL_GLContext& out_context);

    // Cleans up the EGL surface + context associated with the given window.
    static void cleanup_egl_context(SDL_Window* window);

    // Swaps buffers for the EGL surface bound to the provided window.
    static void swap_buffers(SDL_Window* window);

    // Makes the EGL context belonging to the given window current on this thread.
    static void make_current(SDL_Window* window, SDL_GLContext context);
    
private:
    static EGLDisplay s_eglDisplay;
    static EGLConfig  s_eglConfig;
    static bool       s_initialized;

    // Per-window maps
    static std::unordered_map<SDL_Window*, EGLSurface> s_surfaces;
    static std::unordered_map<SDL_Window*, EGLContext> s_contexts;

    static bool initialize_egl_display();
    static bool choose_egl_config();
    static EGLSurface create_egl_surface(SDL_Window* window);
    static EGLContext create_egl_context(EGLSurface surface);
};

#endif // EGL_COMPATIBILITY_H 