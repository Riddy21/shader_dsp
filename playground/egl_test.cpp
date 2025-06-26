#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <iostream>
#include <string>

int main() {
    std::cout << "=== EGL/OpenGL ES Test ===" << std::endl;
    
    // Get EGL display
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        std::cerr << "Failed to get EGL display" << std::endl;
        return 1;
    }
    
    // Initialize EGL
    EGLint major, minor;
    if (!eglInitialize(display, &major, &minor)) {
        std::cerr << "Failed to initialize EGL" << std::endl;
        return 1;
    }
    
    std::cout << "EGL Version: " << major << "." << minor << std::endl;
    std::cout << "EGL Vendor: " << eglQueryString(display, EGL_VENDOR) << std::endl;
    std::cout << "EGL Version String: " << eglQueryString(display, EGL_VERSION) << std::endl;
    std::cout << "EGL Client APIs: " << eglQueryString(display, EGL_CLIENT_APIS) << std::endl;
    std::cout << "EGL Extensions: " << eglQueryString(display, EGL_EXTENSIONS) << std::endl;
    
    // Check for OpenGL ES 2.0 support
    const EGLint es2_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    
    EGLConfig config;
    EGLint num_configs;
    if (eglChooseConfig(display, es2_attribs, &config, 1, &num_configs) && num_configs > 0) {
        std::cout << "✓ OpenGL ES 2.0 supported" << std::endl;
    } else {
        std::cout << "✗ OpenGL ES 2.0 not supported" << std::endl;
    }
    
    // Check for OpenGL ES 3.0 support
    const EGLint es3_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE
    };
    
    if (eglChooseConfig(display, es3_attribs, &config, 1, &num_configs) && num_configs > 0) {
        std::cout << "✓ OpenGL ES 3.0 supported" << std::endl;
    } else {
        std::cout << "✗ OpenGL ES 3.0 not supported" << std::endl;
    }
    
    // List all available configs
    EGLint total_configs;
    eglGetConfigs(display, nullptr, 0, &total_configs);
    std::cout << "Total EGL configs available: " << total_configs << std::endl;
    
    eglTerminate(display);
    return 0;
} 