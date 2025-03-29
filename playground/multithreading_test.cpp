#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <thread>
#include <vector>

// Function to render in a thread
void render_loop(GLFWwindow* window) {
    // Make the OpenGL context current for this thread
    glfwMakeContextCurrent(window);

    // Rendering loop
    while (!glfwWindowShouldClose(window)) {
        // Clear the screen
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw a triangle
        glBegin(GL_TRIANGLES);
        glColor3f(1.0f, 0.0f, 0.0f);
        glVertex2f(-0.5f, -0.5f);
        glColor3f(0.0f, 1.0f, 0.0f);
        glVertex2f(0.5f, -0.5f);
        glColor3f(0.0f, 0.0f, 1.0f);
        glVertex2f(0.0f, 0.5f);
        glEnd();

        // Swap buffers
        glfwSwapBuffers(window);

        // Poll for events
        glfwPollEvents();
    }

    // Destroy the window
    glfwDestroyWindow(window);
}

int main() {
    // Initialize GLFW on the main thread
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Create windows on the main thread
    GLFWwindow* window1 = glfwCreateWindow(800, 600, "Window 1", nullptr, nullptr);
    if (!window1) {
        std::cerr << "Failed to create GLFW window 1" << std::endl;
        glfwTerminate();
        return -1;
    }

    GLFWwindow* window2 = glfwCreateWindow(500, 500, "Window 2", nullptr, nullptr);
    if (!window2) {
        std::cerr << "Failed to create GLFW window 2" << std::endl;
        glfwDestroyWindow(window1);
        glfwTerminate();
        return -1;
    }

    // Create threads for rendering
    std::thread thread1(render_loop, window1);
    std::thread thread2(render_loop, window2);

    // Wait for threads to finish
    thread1.join();
    thread2.join();

    // Terminate GLFW
    glfwTerminate();

    return 0;
}