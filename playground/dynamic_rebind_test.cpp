#include <GL/glew.h>
#include <GL/freeglut.h>
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

// We'll make the gradient slightly different if "usingTexB" is true
uniform float uUseTexB; 

void main() {
    // Basic gradient: (vTexCoord.x, vTexCoord.y, 0.5)
    vec3 base = vec3(vTexCoord.x, vTexCoord.y, 0.5);

    // If usingTexB=1.0, shift color a bit to distinguish
    // e.g. add some green or something
    if (uUseTexB > 0.5) {
        base.g += 0.3;  // a small green tint
    } else {
        base.r += 0.3;  // a small red tint
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

// -----------------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------------
static void initGL() {
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (GLEW_OK != err) {
        std::cerr << "GLEW init error: " << glewGetErrorString(err) << std::endl;
    }

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
static void display() {
    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);

    // -------------------------------------------------------------
    // Pass 1: Render to FBO, dynamically attaching colorTexA or colorTexB
    // -------------------------------------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Choose which texture to attach
    GLuint chosenTex = (useTextureB ? colorTexB : colorTexA);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, chosenTex, 0);

    // [Optional] Check completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "FBO incomplete!" << std::endl;
    }

    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);

    // Use our first-pass program
    glUseProgram(programFirstPass);

    // Set a uniform so we know if we're using TexB
    GLint locUseTexB = glGetUniformLocation(programFirstPass, "uUseTexB");
    glUniform1f(locUseTexB, (useTextureB ? 1.0f : 0.0f));

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // -------------------------------------------------------------
    // Pass 2: Render to the default framebuffer (the screen),
    // sampling whichever texture we used
    // -------------------------------------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(programDisplay);

    GLint locTex = glGetUniformLocation(programDisplay, "uTexture");
    glUniform1i(locTex, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, chosenTex);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Done
    glutSwapBuffers();
}

// -----------------------------------------------------------------------------
// Reshape
// -----------------------------------------------------------------------------
static void reshape(int w, int h) {
    // Re-create textures for new size
    if (colorTexA) glDeleteTextures(1, &colorTexA);
    if (colorTexB) glDeleteTextures(1, &colorTexB);

    createTextures(w, h);
    glViewport(0, 0, w, h);
}

// -----------------------------------------------------------------------------
// Keyboard
// -----------------------------------------------------------------------------
static void keyboard(unsigned char key, int x, int y) {
    if (key == 'p' || key == 'P') {
        useTextureB = !useTextureB;
        std::cout << "Now using " << (useTextureB ? "texture B" : "texture A") 
                  << " for the FBO.\n";
        glutPostRedisplay();
    }
    else if (key == 27) { // ESC
        glutLeaveMainLoop();
    }
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    // Init GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Dynamic FBO Texture Attachment");

    // Initialize GL context (GLEW, etc.)
    initGL();

    // Create offscreen textures & FBO
    createTextures(800, 600);
    createFBO();

    // Register callbacks
    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);

    // Main loop
    glutMainLoop();
    return 0;
}
