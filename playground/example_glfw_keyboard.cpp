#include <GLFW/glfw3.h>
#include <iostream>

// Callback function for key events
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        std::cout << "Key pressed: " << key << std::endl;
    } else if (action == GLFW_RELEASE) {
        std::cout << "Key released: " << key << std::endl;
    }
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Create a windowed mode window and its OpenGL context
    GLFWwindow* window = glfwCreateWindow(640, 480, "GLFW Keyboard Example", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Make the window's context current
    glfwMakeContextCurrent(window);

    // Set the key callback
    glfwSetKeyCallback(window, key_callback);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Render here (if needed)
        glClear(GL_COLOR_BUFFER_BIT);

        // Poll for and process events
        glfwPollEvents();

        // Swap front and back buffers
        glfwSwapBuffers(window);
    }

    // Clean up and exit
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
