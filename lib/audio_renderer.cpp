#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <thread>
#include <condition_variable>

#include "audio_renderer.h"

AudioRenderer * AudioRenderer::instance = nullptr;

bool AudioRenderer::add_render_stage(std::unique_ptr<AudioRenderStage> render_stage)
{
    if (m_initialized) {
        std::cerr << "Error: Cannot add render stage after initialization." << std::endl;
        return false;
    }
    // Add the render stage to the list of render stages
    m_render_stages.push_back(std::move(render_stage));
    m_num_stages = m_render_stages.size();

    // link the render stage to the audio renderer
    m_render_stages.back()->link_renderer(this);

    // Link the render stage to the previous render stage by finding the stream_audio_texture
    if (m_num_stages > 1) {
        // TODO: Encapsulate this in a function or class
        // FIXME: Enforce the requirement of an output_audio_texture and a stream_audio_texture
        auto output_audio_texture = m_render_stages[m_num_stages - 2]->find_parameter("output_audio_texture");
        auto stream_audio_texture = m_render_stages[m_num_stages - 1]->find_parameter("stream_audio_texture");
        output_audio_texture->link(stream_audio_texture);
    }

    
    return true;
}

bool AudioRenderer::add_render_output(std::unique_ptr<AudioOutputNew> output_link)
{
    m_render_outputs.push_back(std::move(output_link));

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

bool AudioRenderer::initialize(const unsigned int buffer_size, const unsigned int sample_rate, const unsigned int num_channels) {
    if (m_initialized) {
        std::cerr << "Error: Audio renderer already initialized." << std::endl;
        return false;
    }

    this->m_buffer_size = buffer_size;
    this->m_num_channels = num_channels;
    this->m_sample_rate = sample_rate;

    // Check that at least one stage has been added
    if (m_num_stages == 0) {
        std::cerr << "Error: No render stages added." << std::endl;
        return false;
    }

    // Initialize GLUT
    initialize_glut(buffer_size*num_channels, 200);
    
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

    // Find the time parameters
    if (!initialize_time_parameters()) {
        std::cerr << "Failed to initialize time parameters." << std::endl;
        return false;
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
    // Stop the loop
    m_running = false;

    // Terminate the OpenGL context
    glutLeaveMainLoop();
    //glutDestroyWindow(glutGetWindow()); // Don't need this

    // Clean up the OpenGL context
    if (!cleanup()) {
        std::cerr << "Failed to clean up OpenGL context." << std::endl;
        return false;
    }
    m_render_outputs.clear();

    // Delete the render stages
    m_render_stages.clear();

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
    // Set the time for the frame
    set_all_time_parameters(m_frame_count);

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

    glReadPixels(0, 0, m_buffer_size * m_num_channels, 1, GL_RED, GL_FLOAT, 0);
    const float * output_buffer_data = (float *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    if (output_buffer_data) {
        push_data_to_all_output_buffers(output_buffer_data);
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

    calculate_frame_rate();
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

AudioRenderer::~AudioRenderer()
{
    // Clean up the OpenGL context
    if (!cleanup()) {
        std::cerr << "Failed to clean up OpenGL context." << std::endl;
    }
    m_render_outputs.clear();

    // Delete the render stages
    m_render_stages.clear();
}

void AudioRenderer::push_data_to_all_output_buffers(const float * data)
{
    for (auto& output : m_render_outputs) {
        if (output->is_ready()) {
            output->push(data);
        }
    }
}

bool AudioRenderer::initialize_time_parameters()
{
    // Initialize the time parameters
    unsigned int counter = 0;
    for (auto& stage : m_render_stages) {
        auto time_param = stage->find_parameter("time");
        if (time_param == nullptr) {
            printf("Error: Time parameter not found in render stage %d\n", counter);
            return false;
        }
        m_frame_time_parameters.push_back(time_param);
        counter ++;
    }
    return true;
}

void AudioRenderer::set_all_time_parameters(const unsigned int time)
{
    // Set the time parameter for all render stages
    for (auto& param : m_frame_time_parameters) {
        param->set_value(&time);
    }
}

AudioRenderStage * AudioRenderer::find_render_stage(const unsigned int gid) {
    for (auto &stage : m_render_stages) {
        if (stage->gid == gid) {
            printf("Found render stage %d\n", gid);
            return stage.get();
        }
    }
    printf("Error: Render stage not found\n");
    return nullptr;
}

AudioOutputNew * AudioRenderer::find_render_output(const unsigned int gid) {
    for (auto &output : m_render_outputs) {
        if (output->gid == gid) {
            printf("Found output link %d\n", gid);
            return output.get();
        }
    }
    printf("Error: Output link not found\n");
    return nullptr;
}