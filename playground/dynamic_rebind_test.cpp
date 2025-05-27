#include <GL/glew.h>
#include <SDL2/SDL.h>
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

// SDL Globals
static SDL_Window* windowA = nullptr;
static SDL_Window* windowB = nullptr;
static SDL_GLContext contextA = nullptr;
static SDL_GLContext contextB = nullptr;

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
        return 0;
    }

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
// Initialization
// -----------------------------------------------------------------------------
static void initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL init error: " << SDL_GetError() << std::endl;
        exit(EXIT_FAILURE);
    }

    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // Create two windows
    windowA = SDL_CreateWindow("Window A", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    windowB = SDL_CreateWindow("Window B", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    if (!windowA || !windowB) {
        std::cerr << "SDL window creation error: " << SDL_GetError() << std::endl;
        exit(EXIT_FAILURE);
    }

    // Create OpenGL contexts
    contextA = SDL_GL_CreateContext(windowA);
    contextB = SDL_GL_CreateContext(windowB);

    if (!contextA || !contextB) {
        std::cerr << "SDL context creation error: " << SDL_GetError() << std::endl;
        exit(EXIT_FAILURE);
    }

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (GLEW_OK != err) {
        std::cerr << "GLEW init error: " << glewGetErrorString(err) << std::endl;
        exit(EXIT_FAILURE);
    }

    // Enable VSync
    SDL_GL_SetSwapInterval(1);
}

static void initGL() {
    // Create programs
    programFirstPass = createProgram(vsSource, fsFirstPass);
    programDisplay   = createProgram(vsSource, fsDisplay);

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
static void renderToContext(SDL_Window* window, SDL_GLContext context, const GLfloat* clearColor, GLuint targetTexture, bool isWindowA) {
    SDL_GL_MakeCurrent(window, context);

    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    if (isWindowA) {
        // Render a moving solid color for windowA
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, w, h);
        glClearColor(0.5f + 0.5f * sin(offsetX), 0.0f, 0.0f, 1.0f); // Moving red intensity
        glClear(GL_COLOR_BUFFER_BIT);
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

    SDL_GL_SwapWindow(window);
}

static void display() {
    // Update the offset for windowA
    offsetX += offsetSpeed;
    // Update animation time for windowB
    animTime += animSpeed;

    // Render a moving graphic to windowA
    renderToContext(windowA, contextA, clearColorA, colorTexA, true);

    // Render the triangle and gradient to the currently selected texture,
    // then display that texture in windowB.
    GLuint targetTex = useTextureB ? colorTexB : colorTexA;
    renderToContext(windowB, contextB, clearColorB, targetTex, false);
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
    SDL_GL_DeleteContext(contextA);
    SDL_GL_DeleteContext(contextB);
    SDL_DestroyWindow(windowA);
    SDL_DestroyWindow(windowB);
    SDL_Quit();

    return 0;
}
