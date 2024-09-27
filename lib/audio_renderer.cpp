#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <thread>

#include "audio_renderer.h"

AudioRenderer * AudioRenderer::instance = nullptr;

bool AudioRenderer::add_render_stage(AudioRenderStage& render_stage)
{
    if (m_initialized) {
        std::cerr << "Error: Cannot add render stage after initialization." << std::endl;
        return false;
    }
    // Add the render stage to the list of render stages
    // FIXME: Change this to unique pointer
    m_render_stages.push_back(std::shared_ptr<AudioRenderStage>(&render_stage, [](AudioRenderStage*){}));
    m_num_stages = m_render_stages.size();

    // link the render stage to the audio renderer
    m_render_stages.back()->link_renderer(this);

    return true;
}

void initialize_glut(unsigned int window_width, unsigned int window_height) {
    printf("Initializing GLUT\n");

    int argc = 0;
    char** argv = nullptr;
    glutInit(&argc, argv);

    // Init the OpenGL context
    glutInitDisplayMode(GLUT_RGBA | GLUT_SINGLE);
    glutInitWindowSize(window_width, window_height);
    //glutInitWindowSize(buffer_size, 100);
    glutCreateWindow("Audio Processing");
    // Set the window close action to continue execution
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);
}

bool create_quad(GLuint &VAO, GLuint &VBO, GLuint &PBO, const unsigned int num_channels, const unsigned int buffer_size) {
    // Just a default set of vertices to cover the screen
    GLfloat vertices[] = {
        // Position    Texcoords
        -1.0f, -1.0f, 0.0f, 1.0f,  // Bottom-left
        -1.0f,  1.0f, 0.0f, 0.0f, // Top-left
         1.0f, -1.0f, 1.0f, 1.0f, // Bottom-right
         1.0f,  1.0f, 1.0f, 0.0f, // Top-right
        -1.0f,  1.0f, 0.0f, 0.0f, // Top-left
         1.0f, -1.0f, 1.0f, 1.0f // Bottom-right
    };

    // Generate Textures and Framebuffers

    // Generate Buffer Objects
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &PBO); // 2 PBOs for audio data

    // Bind Vertex Objects
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // Bind Pixel buffers
    glBindBuffer(GL_PIXEL_PACK_BUFFER, PBO);

    // Allocate memory for vertex data
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Allocate memory for the audio data
    glBufferData(GL_PIXEL_PACK_BUFFER, buffer_size * num_channels * sizeof(float), nullptr, GL_STREAM_READ);

    // Configure the vertex attributes
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)0); // Vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)(2 * sizeof(GLfloat))); // Texture attributes
    glEnableVertexAttribArray(1);

    // Unbind the buffers
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "Failed to initialize OpenGL: " << error << std::endl;
        return false;
    }
    return true;
}

bool AudioRenderer::init(const unsigned int buffer_size, const unsigned int sample_rate, const unsigned int num_channels) {
    if (m_initialized) {
        std::cerr << "Error: Audio renderer already initialized." << std::endl;
        return false;
    }

    this->m_buffer_size = buffer_size;
    this->m_num_channels = num_channels;
    this->m_sample_rate = sample_rate;
    this->m_delay = 1000 * buffer_size / sample_rate;

    // Check that at least one stage has been added
    if (m_num_stages == 0) {
        std::cerr << "Error: No render stages added." << std::endl;
        return false;
    }

    // Initialize GLUT
    initialize_glut(buffer_size*num_channels, 200);
    
    // Initialize GLEW
    GLenum glewInitResult = glewInit();
    if (glewInitResult != GLEW_OK) {        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(glewInitResult) << std::endl;
        return false;
    }

    // Print the OpenGL version
    const GLubyte* glVersion = glGetString(GL_VERSION);
    const GLubyte* glslVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);
    std::cout << "OpenGL Version: " << glVersion << std::endl;
    std::cout << "GLSL Version: " << glslVersion << std::endl;

    // Set GL settings for audio rendering
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);

    if (m_render_stages.size() == 0) {
        std::cerr << "Error: No render stages added." << std::endl;
        return false;
    }

    // initialize all the render stages
    for (auto& stage : m_render_stages) {
        if (!stage->initialize_shader_stage()) {
            std::cerr << "Failed to compile render stages" << std::endl;
            return false;
        }
    }

    // Link parameters
    for (auto& stage : m_render_stages) {
        if (!stage->bind_shader_stage()) {
            std::cerr << "Failed to link render stages" << std::endl;
            return false;
        }
    }

    // set the time parameter
    for (auto& stage : m_render_stages) {
        if (m_frame_time_parameter == nullptr) {
            m_frame_time_parameter = stage->find_parameter("time");
            printf("Found time parameter\n");
        } else {
            break;
        }
    }

    // Create the quad
    if (!create_quad(m_VAO, m_VBO, m_PBO, num_channels, buffer_size)) {
        std::cerr << "Failed to create quad." << std::endl;
        return false;
    }

    // Initialize the textures with data
    render();

    // Links to display and timer functions for updating audio data
    glutIdleFunc(render_callback);
    glutTimerFunc(m_delay, timer_callback, m_frame_count++);
    glutDisplayFunc(display_callback);

    m_initialized = true;

    return true;
}

void AudioRenderer::calculate_frame_rate()
{
    static int frame_count = 0;
    static double previous_time = 0.0;
    static double fps = 0.0;

    frame_count++;
    double current_time = glutGet(GLUT_ELAPSED_TIME) / 1000.0; // Get current time in seconds
    double elapsed_time = current_time - previous_time;

    if (elapsed_time > 1.0) { // Update FPS every second
        fps = frame_count / elapsed_time;
        previous_time = current_time;
        frame_count = 0;
    }

    // Display the FPS on the window title
    char title[256];
    sprintf(title, "Audio Processing - FPS: %.2f", fps);
    glutSetWindowTitle(title);
}

void AudioRenderer::render()
{
    m_audio_data_mutex.lock(); // Lock the mutex

    glBindVertexArray(m_VAO);

    for (int i = 0; i < (int)m_num_stages; i++) {
        // Render the stage
        m_render_stages[i]->render_render_stage();
    }

    // Bind the last stage to read the audio data
    glUseProgram(m_render_stages[m_num_stages - 1]->get_shader_program());
    glBindFramebuffer(GL_FRAMEBUFFER, m_render_stages[m_num_stages - 1]->get_framebuffer());

    // Bind the color attachment we are on
    glReadBuffer(GL_COLOR_ATTACHMENT0 + m_render_stages[m_num_stages - 1]->get_color_attachment_count()); // Change to the specific color attachment you want to read from

    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PBO);
    // TODO: Current Frame buffers dependent on size of the screen, need to work without a screen
    glReadPixels(0, 0, m_buffer_size * m_num_channels, 1, GL_RED, GL_FLOAT, 0);
    m_output_buffer_data = (float *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    if (m_output_buffer_data) {
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    // Display to screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Unbind everything
    glBindVertexArray(0);
    glUseProgram(0);

    m_audio_data_mutex.unlock(); // Unlock the mutex

    //calculate_frame_rate();
}

void AudioRenderer::timer(unsigned int frame)
{
    m_audio_data_mutex.lock(); // Lock the mutex

    // Set the time for the frame
    m_frame_time_parameter->set_value(&frame);

    // unlock the mutex
    m_audio_data_mutex.unlock();

    // Set the timer function with the calculated delay
    glutTimerFunc(m_delay, timer_callback, m_frame_count++);

    // Calculate the frame rate
    //calculate_frame_rate();
}

void AudioRenderer::main_loop()
{
    m_running = true;
    start_push_data_to_output_buffers_thread();
    glutMainLoop();
}

bool AudioRenderer::cleanup()
{
    // Delete the vertex array and buffer
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);

    // Delete the pixel buffer object
    glDeleteBuffers(1, &m_PBO);

    return true;
}

bool AudioRenderer::terminate()
{
    // Terminate the OpenGL context
    m_running = false;
    glutLeaveMainLoop();
    //glutDestroyWindow(glutGetWindow()); // Don't need this

    return true;
}

AudioRenderer::~AudioRenderer()
{
    // Clean up the OpenGL context
    if (!cleanup()) {
        std::cerr << "Failed to clean up OpenGL context." << std::endl;
    }
    m_output_buffers.clear();
}

AudioBuffer * AudioRenderer::get_new_output_buffer()
{
    // Create a new output buffer
    std::unique_ptr<AudioBuffer> output_buffer(new AudioBuffer(10, m_num_channels * m_buffer_size));
    m_output_buffers.push_back(std::move(output_buffer));
    
    return m_output_buffers.back().get();
}

void AudioRenderer::start_push_data_to_output_buffers_thread()
{
    // Push the data to all output buffers
    std::thread output_thread[m_output_buffers.size()];
    for (unsigned int i = 0; i < m_output_buffers.size(); i++) {
        // Start a thread
        output_thread[i] = std::thread([this, i](){
            while (m_running) {
                // Push the data to the output buffer
                m_output_buffers[i]->push(m_output_buffer_data);

                calculate_frame_rate();
            }
        });
        output_thread[i].detach();
    }
}
