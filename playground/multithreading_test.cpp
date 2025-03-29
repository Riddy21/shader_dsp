#include <GL/glew.h>
#include <GL/freeglut.h>
#include <thread>
#include <iostream>
#include <X11/Xlib.h> // Include Xlib for XInitThreads

// Function to initialize and run a GLUT window in a thread
void render_loop(const char* window_title, int argc, char** argv, int width, int height) {
    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(width, height);
    glutCreateWindow(window_title);

    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return;
    }

    // Set up the display function
    glutDisplayFunc([]() {
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
        glutSwapBuffers();
    });

    // Set up the idle function to continuously redraw
    glutIdleFunc([]() {
        glutPostRedisplay();
    });

    // Enter the GLUT main loop
    glutMainLoop();
}

int main(int argc, char** argv) {
    // Initialize X for multithreaded use
    if (!XInitThreads()) {
        std::cerr << "Failed to initialize X for multithreaded use" << std::endl;
        return -1;
    }

    // Create two threads for rendering
    std::thread thread1(render_loop, "Window 1", argc, argv, 400, 400);
    std::thread thread2(render_loop, "Window 2", argc, argv, 800, 600);

    // Wait for threads to finish
    thread1.join();
    thread2.join();

    return 0;
}