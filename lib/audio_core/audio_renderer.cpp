#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <thread>
#include <condition_variable>

#include "audio_parameter/audio_uniform_buffer_parameter.h"
#include "audio_render_stage/audio_final_render_stage.h"
#include "audio_core/audio_renderer.h"
#include "audio_core/audio_render_graph.h"

AudioRenderer * AudioRenderer::instance = nullptr;

AudioRenderer::AudioRenderer() {
    // Initialize the global time parameter
    auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
    global_time->set_value(0);
    add_global_parameter(global_time);
}

bool AudioRenderer::add_render_output(AudioOutput * output_link)
{
    m_render_outputs.push_back(std::unique_ptr<AudioOutput>(output_link));

    return true;
}

bool AudioRenderer::add_global_parameter(AudioParameter * parameter)
{
    m_global_parameters.push_back(std::unique_ptr<AudioParameter>(parameter));

    return true;
}

bool AudioRenderer::add_render_graph(AudioRenderGraph * render_graph)
{
    if (m_initialized) {
        std::cerr << "Error: Cannot add render graph after initialization." << std::endl;
        return false;
    }
    m_render_graph = std::unique_ptr<AudioRenderGraph>(render_graph);

    return true;
}

bool AudioRenderer::initialize_glut(unsigned int window_width, unsigned int window_height) {
    printf("Initializing GLUT\n");

    int argc = 0;
    char** argv = nullptr;
    if (!glutGet(GLUT_INIT_STATE)) {
        glutInit(&argc, argv);
    }

    // Init the OpenGL context
    glutInitDisplayMode(GLUT_RGBA | GLUT_SINGLE);
    glutInitWindowSize(window_width, window_height);
    //glutInitWindowSize(buffer_size, 100);
    glutCreateWindow("Audio Processing");
    // Set the window close action to continue execution
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return false;
    }
    return true;
}

bool AudioRenderer::initialize_quad() {
    // Just a default set of vertices to cover the screen
    GLfloat vertices[] = {
        // Position    Texcoords
        -1.0f, -1.0f, 0.0f, 0.0f,  // Bottom-left
        -1.0f,  1.0f, 0.0f, 1.0f,  // Top-left
         1.0f, -1.0f, 1.0f, 0.0f,  // Bottom-right
         1.0f,  1.0f, 1.0f, 1.0f,  // Top-right
        -1.0f,  1.0f, 0.0f, 1.0f,  // Top-left
         1.0f, -1.0f, 1.0f, 0.0f   // Bottom-right
    };

    // Generate Textures and Framebuffers

    // Generate Buffer Objects
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    // Bind Vertex Objects
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);

    // Allocate memory for vertex data
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

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

bool AudioRenderer::initialize(const unsigned int buffer_size, const unsigned int sample_rate, const unsigned int num_channels) {
    if (m_initialized) {
        std::cerr << "Error: Audio renderer already initialized." << std::endl;
        return false;
    }

    this->m_buffer_size = buffer_size;
    this->m_num_channels = num_channels;
    this->m_sample_rate = sample_rate;

    // Initialize GLUT
    initialize_glut(buffer_size, num_channels);
    
    // Initialize GLEW
    GLenum glewInitResult = glewInit();
    if (glewInitResult != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(glewInitResult) << std::endl;
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

    // Check if render graph is added
    if (m_render_graph == nullptr) {
        std::cerr << "Error: No render graph added." << std::endl;
        return false;
    }

    // Initialize the render stages
    if (!m_render_graph->initialize()) {
        std::cerr << "Failed to initialize render graph." << std::endl;
        return false;
    }

    // Intialize global parameters
    if (!initialize_global_parameters()) {
        std::cerr << "Failed to initialize time parameters." << std::endl;
        return false;
    }

    // Create the quad
    if (!initialize_quad()) {
        std::cerr << "Failed to create quad." << std::endl;
        return false;
    }

    // Initialize the textures with data
    render();

    // Links to display and timer functions for updating audio data
    glutIdleFunc(render_callback);
    glutDisplayFunc(display_callback);

    m_initialized = true;


    return true;
}

void AudioRenderer::start_main_loop() {
    if (m_initialized) {
        m_running = true;

        glutMainLoop();
    }
}

bool AudioRenderer::terminate() {
    // Terminate the OpenGL context
    glutLeaveMainLoop();
    //glutDestroyWindow(glutGetWindow()); // Don't need this

    // Stop the loop
    m_running = false;
    m_initialized = false;

    // Clean up the OpenGL context
    if (!cleanup()) {
        std::cerr << "Failed to clean up OpenGL context." << std::endl;
        return false;
    }


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
    m_render_graph->bind();

    // Set the time for the frame
    // TODO: Encapsulate in a function once we have more parameters
    auto time_param = find_global_parameter("global_time");
    time_param->set_value(m_frame_count);

    glBindVertexArray(m_VAO);

    // render the global parameters
    for (auto& param : m_global_parameters) {
        param->render();
    }

    static unsigned int prev_time = 0;

    if (m_frame_count == 0 || m_frame_count != prev_time) {
        // Render the render graph
        m_render_graph->render(m_frame_count);

        calculate_frame_rate();
    } else {
        // TODO: Do something else instead of sleep here, come up with way to save some compute
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    prev_time = m_frame_count;


    // Push to output buffers
    push_to_output_buffers(m_render_graph->get_output_render_stage()->get_output_buffer_data());

    // Unbind everything
    glBindVertexArray(0);
}


bool AudioRenderer::cleanup()
{
    // Delete the vertex array and buffer
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);

    m_render_outputs.clear();
    m_global_parameters.clear();
    m_render_graph.reset();
    m_initialized = false;
    m_running = false;
    m_paused = false;
    m_frame_count = 0;
    m_lead_output = nullptr;

    return true;
}

AudioRenderer::~AudioRenderer()
{
    // Clean up the OpenGL context
    if (!cleanup()) {
        std::cerr << "Failed to clean up OpenGL context." << std::endl;
    }
}

void AudioRenderer::push_to_output_buffers(const float * data)
{
    // Check if there is outputs to push to 
    if (m_render_outputs.size() == 0) {
        std::cerr << "Error: No outputs to push to." << std::endl;
        return;
    }

    if (m_render_outputs.size() == 0) {
        return;
    }

    // Set the lead output to the first one on the list by default
    if (m_lead_output == nullptr) {
        m_lead_output = m_render_outputs[0].get();
    }

    if (!m_lead_output->is_ready()) {
        return;
    }

    m_frame_count++;

    for (auto& output : m_render_outputs) {
        output->push(data);
    }
}

bool AudioRenderer::initialize_global_parameters()
{
    for (auto& param : m_global_parameters) {
        if (!param->initialize()) {
            std::cerr << "Failed to initialize global parameters" << std::endl;
            return false;
        }
    }
    return true;
}

AudioOutput * AudioRenderer::find_render_output(const unsigned int gid) {
    for (auto &output : m_render_outputs) {
        if (output->gid == gid) {
            printf("Found output link %d\n", gid);
            return output.get();
        }
    }
    printf("Error: Output link not found\n");
    return nullptr;
}

AudioParameter * AudioRenderer::find_global_parameter(const std::string name) const {
    for (auto &param : m_global_parameters) {
        if (param->name == name) {
            return param.get();
        }
    }
    return nullptr;
}