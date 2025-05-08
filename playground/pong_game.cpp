#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <iostream>

// Paddle and ball properties
struct Paddle {
    float x, y;
    float width = 0.1f, height = 0.3f;
    float speed = 0.02f;
};

struct Ball {
    float x, y;
    float radius = 0.05f;
    float speedX = 0.01f, speedY = 0.01f;
};

// Function to draw a rectangle
void draw_rectangle(float x, float y, float width, float height) {
    glBegin(GL_QUADS);
    glVertex2f(x - width / 2, y - height / 2);
    glVertex2f(x + width / 2, y - height / 2);
    glVertex2f(x + width / 2, y + height / 2);
    glVertex2f(x - width / 2, y + height / 2);
    glEnd();
}

// Function to draw a circle
void draw_circle(float x, float y, float radius) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for (int i = 0; i <= 360; i += 10) {
        float angle = i * 3.14159f / 180.0f;
        glVertex2f(x + cos(angle) * radius, y + sin(angle) * radius);
    }
    glEnd();
}

int main() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // Create SDL window
    SDL_Window* window = SDL_CreateWindow("Pong Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    // Create OpenGL context
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Game objects
    Paddle leftPaddle = {-0.9f, 0.0f};
    Paddle rightPaddle = {0.9f, 0.0f};
    Ball ball = {0.0f, 0.0f};

    // Main game loop
    bool running = true;
    SDL_Event event;
    while (running) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        // Handle paddle movement
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        if (keys[SDL_SCANCODE_W] && leftPaddle.y + leftPaddle.height / 2 < 1.0f) {
            leftPaddle.y += leftPaddle.speed;
        }
        if (keys[SDL_SCANCODE_S] && leftPaddle.y - leftPaddle.height / 2 > -1.0f) {
            leftPaddle.y -= leftPaddle.speed;
        }
        if (keys[SDL_SCANCODE_UP] && rightPaddle.y + rightPaddle.height / 2 < 1.0f) {
            rightPaddle.y += rightPaddle.speed;
        }
        if (keys[SDL_SCANCODE_DOWN] && rightPaddle.y - rightPaddle.height / 2 > -1.0f) {
            rightPaddle.y -= rightPaddle.speed;
        }

        // Update ball position
        ball.x += ball.speedX;
        ball.y += ball.speedY;

        // Ball collision with top and bottom walls
        if (ball.y + ball.radius > 1.0f || ball.y - ball.radius < -1.0f) {
            ball.speedY = -ball.speedY;
        }

        // Ball collision with paddles
        if ((ball.x - ball.radius < leftPaddle.x + leftPaddle.width / 2 &&
             ball.y > leftPaddle.y - leftPaddle.height / 2 &&
             ball.y < leftPaddle.y + leftPaddle.height / 2) ||
            (ball.x + ball.radius > rightPaddle.x - rightPaddle.width / 2 &&
             ball.y > rightPaddle.y - rightPaddle.height / 2 &&
             ball.y < rightPaddle.y + rightPaddle.height / 2)) {
            ball.speedX = -ball.speedX;
        }

        // Ball out of bounds (reset position)
        if (ball.x + ball.radius > 1.0f || ball.x - ball.radius < -1.0f) {
            ball.x = 0.0f;
            ball.y = 0.0f;
            ball.speedX = 0.01f * (ball.speedX > 0 ? -1 : 1); // Reverse direction
        }

        // Clear the screen
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black background
        glClear(GL_COLOR_BUFFER_BIT);

        // Render paddles and ball
        glColor3f(1.0f, 1.0f, 1.0f); // White color
        draw_rectangle(leftPaddle.x, leftPaddle.y, leftPaddle.width, leftPaddle.height);
        draw_rectangle(rightPaddle.x, rightPaddle.y, rightPaddle.width, rightPaddle.height);
        draw_circle(ball.x, ball.y, ball.radius);

        // Swap buffers
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
