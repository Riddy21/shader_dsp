#pragma once
#ifndef EGL_COMPATIBILITY_H
#define EGL_COMPATIBILITY_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <iostream>
#include <stdexcept>

// EGL compatibility layer for OpenGL ES context creation
class EGLCompatibility {
public:
    static bool initialize_egl_context(SDL_Window* window, SDL_GLContext& out_context);
    static void cleanup_egl_context(SDL_GLContext context);
    static void swap_buffers(SDL_Window* window);
    static void make_current(SDL_Window* window, SDL_GLContext context);
    
private:
    static EGLDisplay s_eglDisplay;
    static EGLSurface s_eglSurface;
    static EGLContext s_eglContext;
    static EGLConfig s_eglConfig;
    static bool s_initialized;
    
    static bool initialize_egl_display();
    static bool choose_egl_config();
    static bool create_egl_surface(SDL_Window* window);
    static bool create_egl_context();
};

#endif // EGL_COMPATIBILITY_H 