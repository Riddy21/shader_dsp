#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <iostream>

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------
static GLuint fbo         = 0;
static GLuint colorTexA   = 0;
static GLuint colorTexB   = 0;
static GLuint vao         = 0;
static GLuint vbo         = 0;

// First-pass program (draws a gradient)
static GLuint programFirstPass = 0;
// Second-pass program (just displays the chosen texture)
static GLuint programDisplay   = 0;
// Modified display program for window A
static GLuint programDisplayWindowA = 0;

// Toggle which texture to attach to the FBO
static bool useTextureB = false;

// A simple fullscreen quad
static const GLfloat quadVertices[] = {
    //  Position    //  Texcoords
    -1.f, -1.f,     0.f, 0.f,
     1.f, -1.f,     1.f, 0.f,
    -1.f,  1.f,     0.f, 1.f,

    -1.f,  1.f,     0.f, 1.f,
     1.f, -1.f,     1.f, 0.f,
     1.f,  1.f,     1.f, 1.f
};

// SDL and EGL Globals
static SDL_Window* windowA = nullptr;
static SDL_Window* windowB = nullptr;

// EGL objects - shared between windows
static EGLDisplay eglDisplay = EGL_NO_DISPLAY;
static EGLSurface eglSurfaceA = EGL_NO_SURFACE;
static EGLSurface eglSurfaceB = EGL_NO_SURFACE;
static EGLContext eglContext = EGL_NO_CONTEXT;
static EGLConfig eglConfig = nullptr;

// Starting clear colors for each context
static const GLfloat clearColorA[] = {0.2f, 0.2f, 0.2f, 1.0f};
static const GLfloat clearColorB[] = {0.2f, 0.2f, 0.5f, 1.0f};

// Offset for moving the graphic on windowA
static float offsetX = 0.0f;
static float offsetSpeed = 0.01f;

// Add a time variable for animation
static float animTime = 0.0f;
static float animSpeed = 0.016f; // ~60fps

// -----------------------------------------------------------------------------
// Shaders
// -----------------------------------------------------------------------------

// Common vertex shader (pass-through)
static const char* vsSource = R"(
#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

// First-pass fragment shader:
// Draws a gradient, plus uses a uniform to shift the color
static const char* fsFirstPass = R"(
#version 300 es
precision mediump float;

in vec2 vTexCoord;
out vec4 FragColor;

uniform float uUseTexB; 
uniform float uTime;

// Function to draw a moving triangle mask
float movingTriangle(vec2 uv, float t) {
    // Move triangle horizontally with time
    uv.x -= 0.3 + 0.3 * sin(t);
    uv.y -= 0.5;
    float a = step(0.0, uv.x + uv.y);
    float b = step(0.0, -uv.x + uv.y);
    float c = step(0.0, 0.3 - uv.y);
    return a * b * c;
}

void main() {
    // Basic gradient: (vTexCoord.x, vTexCoord.y, 0.5)
    vec3 base = vec3(vTexCoord.x, vTexCoord.y, 0.5);

    // Animate color shift for texture A
    if (uUseTexB < 0.5) {
        base.r += 0.3 + 0.2 * sin(uTime);
        base.g += 0.2 * cos(uTime * 0.7);
    } else {
        base.g += 0.3;  // a small green tint
    }

    // Overlay a moving triangle as a solid yellow shape
    float tri = movingTriangle(vTexCoord, uTime);
    base = mix(base, vec3(1.0, 1.0, 0.2), tri);

    // Ensure we have a visible color even if uniforms fail
    if (length(base) < 0.1) {
        base = vec3(1.0, 0.0, 0.0); // Fallback to red
    }

    FragColor = vec4(base, 1.0);
}
)";

// Second-pass fragment shader:
// Samples the texture and outputs it directly
static const char* fsDisplay = R"(
#version 300 es
precision mediump float;

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;

void main() {
    FragColor = texture(uTexture, vTexCoord);
}
)";

// Modified display shader for window A - inverts colors and adds a border
static const char* fsDisplayWindowA = R"(
#version 300 es
precision mediump float;

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;

void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    
    // Invert the colors
    vec3 inverted = 1.0 - texColor.rgb;
    
    // Add a subtle border effect
    float border = 0.05;
    float edge = smoothstep(0.0, border, vTexCoord.x) * 
                 smoothstep(1.0, 1.0 - border, vTexCoord.x) *
                 smoothstep(0.0, border, vTexCoord.y) * 
                 smoothstep(1.0, 1.0 - border, vTexCoord.y);
    
    // Mix inverted color with border
    vec3 finalColor = mix(inverted, vec3(0.8, 0.8, 0.8), 1.0 - edge);
    
    FragColor = vec4(finalColor, texColor.a);
}
)";

// -----------------------------------------------------------------------------
// Helper: compile & link
// -----------------------------------------------------------------------------
static GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        char* log = new char[len];
        glGetShaderInfoLog(shader, len, &len, log);
        std::cerr << "Shader compile error:\n" << log << std::endl;
        delete[] log;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint createProgram(const char* vs, const char* fs) {
    GLuint vsObj = compileShader(GL_VERTEX_SHADER, vs);
    GLuint fsObj = compileShader(GL_FRAGMENT_SHADER, fs);
    if (!vsObj || !fsObj) {
        std::cerr << "Failed to compile shaders" << std::endl;
        return 0;
    }

    std::cout << "Shaders compiled successfully" << std::endl;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vsObj);
    glAttachShader(prog, fsObj);
    glLinkProgram(prog);

    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        char* log = new char[len];
        glGetProgramInfoLog(prog, len, &len, log);
        std::cerr << "Program link error:\n" << log << std::endl;
        delete[] log;
        glDeleteProgram(prog);
        return 0;
    }

    std::cout << "Program linked successfully" << std::endl;

    // Clean up
    glDetachShader(prog, vsObj);
    glDetachShader(prog, fsObj);
    glDeleteShader(vsObj);
    glDeleteShader(fsObj);

    return prog;
}

// -----------------------------------------------------------------------------
// Create two textures
// -----------------------------------------------------------------------------
static void createTextures(int w, int h) {
    glGenTextures(1, &colorTexA);
    glBindTexture(GL_TEXTURE_2D, colorTexA);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenTextures(1, &colorTexB);
    glBindTexture(GL_TEXTURE_2D, colorTexB);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Unbind
    glBindTexture(GL_TEXTURE_2D, 0);
}

// -----------------------------------------------------------------------------
// Create a single FBO (we'll attach either texA or texB at runtime)
// -----------------------------------------------------------------------------
static void createFBO() {
    glGenFramebuffers(1, &fbo);
    // We'll attach in the display() function, depending on useTextureB
}

// Helper function to rebind the texture to the FBO
static void rebindFramebufferTexture() {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Choose which texture to attach
    GLuint chosenTex = (useTextureB ? colorTexB : colorTexA);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, chosenTex, 0);

    // [Optional] Check completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "FBO incomplete!" << std::endl;
    }

    // Unbind the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// -----------------------------------------------------------------------------
// EGL initialization helpers
// -----------------------------------------------------------------------------
static bool initializeEGL(EGLDisplay& display, EGLSurface& surface, EGLContext& context, EGLConfig& config, SDL_Window* window) {
    // Get EGL display
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        std::cerr << "EGL: Failed to get EGL display" << std::endl;
        return false;
    }

    // Initialize EGL
    EGLint majorVersion, minorVersion;
    if (!eglInitialize(display, &majorVersion, &minorVersion)) {
        std::cerr << "EGL: Failed to initialize EGL" << std::endl;
        return false;
    }

    // Choose EGL config
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
    if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfigs)) {
        std::cerr << "EGL: Failed to choose EGL config" << std::endl;
        return false;
    }

    if (numConfigs == 0) {
        std::cerr << "EGL: No suitable EGL config found" << std::endl;
        return false;
    }

    // Create EGL surface
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
        std::cerr << "EGL: Failed to get window WM info" << std::endl;
        return false;
    }

    surface = eglCreateWindowSurface(display, config, 
                                   (EGLNativeWindowType)wmInfo.info.x11.window, NULL);
    if (surface == EGL_NO_SURFACE) {
        std::cerr << "EGL: Failed to create EGL surface" << std::endl;
        return false;
    }

    // Create EGL context - share with the first context if it exists
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        std::cerr << "EGL: Failed to create EGL context" << std::endl;
        return false;
    }

    return true;
}

static void cleanupEGL(EGLDisplay& display, EGLSurface& surface, EGLContext& context) {
    if (context != EGL_NO_CONTEXT) {
        eglDestroyContext(display, context);
        context = EGL_NO_CONTEXT;
    }
    
    if (surface != EGL_NO_SURFACE) {
        eglDestroySurface(display, surface);
        surface = EGL_NO_SURFACE;
    }
    
    if (display != EGL_NO_DISPLAY) {
        eglTerminate(display);
        display = EGL_NO_DISPLAY;
    }
}

// -----------------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------------
static void initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL init error: " << SDL_GetError() << std::endl;
        exit(EXIT_FAILURE);
    }

    // Create two windows
    windowA = SDL_CreateWindow("Window A", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN);
    windowB = SDL_CreateWindow("Window B", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN);

    if (!windowA || !windowB) {
        std::cerr << "SDL window creation error: " << SDL_GetError() << std::endl;
        exit(EXIT_FAILURE);
    }

    // Initialize EGL for both windows with shared context
    if (!initializeEGL(eglDisplay, eglSurfaceA, eglContext, eglConfig, windowA)) {
        std::cerr << "Failed to initialize EGL for window A" << std::endl;
        exit(EXIT_FAILURE);
    }
    
    // Create surface for second window using same display and context
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(windowB, &wmInfo)) {
        std::cerr << "EGL: Failed to get window WM info for window B" << std::endl;
        exit(EXIT_FAILURE);
    }

    eglSurfaceB = eglCreateWindowSurface(eglDisplay, eglConfig, 
                                       (EGLNativeWindowType)wmInfo.info.x11.window, NULL);
    if (eglSurfaceB == EGL_NO_SURFACE) {
        std::cerr << "EGL: Failed to create EGL surface for window B" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Make window A context current initially
    if (!eglMakeCurrent(eglDisplay, eglSurfaceA, eglSurfaceA, eglContext)) {
        std::cerr << "EGL: Failed to make context current for window A" << std::endl;
        exit(EXIT_FAILURE);
    }
}

static void initGL() {
    // Create programs
    programFirstPass = createProgram(vsSource, fsFirstPass);
    programDisplay   = createProgram(vsSource, fsDisplay);
    programDisplayWindowA = createProgram(vsSource, fsDisplayWindowA);

    if (!programFirstPass || !programDisplay || !programDisplayWindowA) {
        std::cerr << "Failed to create shader programs" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Shader programs created successfully" << std::endl;

    // Setup VAO/VBO
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(0)
    );
    // TexCoord attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float))
    );

    // Unbind
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Set a nice clear color
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
}

// -----------------------------------------------------------------------------
// Display function
// -----------------------------------------------------------------------------
static void renderToContext(SDL_Window* window, EGLDisplay display, EGLSurface surface, EGLContext context, const GLfloat* clearColor, GLuint targetTexture, bool isWindowA) {
    // Make the context current
    if (!eglMakeCurrent(display, surface, surface, context)) {
        std::cerr << "EGL: Failed to make context current" << std::endl;
        return;
    }

    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error before rendering: 0x" << std::hex << error << std::dec << std::endl;
    }

    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    if (isWindowA) {
        // Render a texture to windowA - use the same texture as window B but with different shader
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, w, h);
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Use a modified display shader for window A
        glUseProgram(programDisplayWindowA);
        GLint locTex = glGetUniformLocation(programDisplayWindowA, "uTexture");
        glUniform1i(locTex, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, targetTexture);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    } else {
        // Pass 1: Render to FBO with the specified target texture
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, targetTexture, 0);
        glViewport(0, 0, w, h);
        glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(programFirstPass);
        GLint locUseTexB = glGetUniformLocation(programFirstPass, "uUseTexB");
        glUniform1f(locUseTexB, (useTextureB ? 1.0f : 0.0f));
        // Pass time to shader
        GLint locTime = glGetUniformLocation(programFirstPass, "uTime");
        glUniform1f(locTime, animTime);

        // Debug: Check if uniforms were set correctly
        if (locUseTexB == -1) {
            std::cerr << "Warning: uUseTexB uniform not found" << std::endl;
        }
        if (locTime == -1) {
            std::cerr << "Warning: uTime uniform not found" << std::endl;
        }

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Pass 2: Render to the default framebuffer (the screen)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(programDisplay);
        GLint locTex = glGetUniformLocation(programDisplay, "uTexture");
        glUniform1i(locTex, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, targetTexture);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    // Swap buffers
    eglSwapBuffers(display, surface);
}

static void display() {
    // Update the offset for windowA
    offsetX += offsetSpeed;
    // Update animation time for windowB
    animTime += animSpeed;

    // Render the triangle and gradient to the currently selected texture
    GLuint targetTex = useTextureB ? colorTexB : colorTexA;
    
    // First, render the gradient to the texture in window B
    renderToContext(windowB, eglDisplay, eglSurfaceB, eglContext, clearColorB, targetTex, false);

    // Then display that texture in window A
    renderToContext(windowA, eglDisplay, eglSurfaceA, eglContext, clearColorA, targetTex, true);
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    // Init SDL and OpenGL
    initSDL();
    initGL();

    // Create offscreen textures & FBO
    createTextures(800, 600);
    createFBO();

    // Main loop
    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_p) {
                    useTextureB = !useTextureB;
                    std::cout << "Now using " << (useTextureB ? "texture B" : "texture A") << " for the FBO.\n";
                } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
            }
        }

        display();
    }

    // Cleanup
    cleanupEGL(eglDisplay, eglSurfaceA, eglContext);
    cleanupEGL(eglDisplay, eglSurfaceB, eglContext);
    SDL_DestroyWindow(windowA);
    SDL_DestroyWindow(windowB);
    SDL_Quit();

    return 0;
}
