#include <SDL2/SDL.h>
#include <GL/glew.h>
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

bool AudioRenderer::initialize_sdl(unsigned int window_width, unsigned int window_height) {
    printf("Initializing SDL2\n");

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL2: " << SDL_GetError() << std::endl;
        return false;
    }

    // Set OpenGL version to 4.1 Core Profile for macOS compatibility
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    m_window = SDL_CreateWindow("Audio Processing", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!m_window) {
        std::cerr << "Failed to create SDL2 window: " << SDL_GetError() << std::endl;
        return false;
    }

    m_gl_context = SDL_GL_CreateContext(m_window);
    if (!m_gl_context) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_GL_SetSwapInterval(0); // Disable vsync

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

    // Initialize SDL2
    if (!initialize_sdl(buffer_size, num_channels)) {
        return false;
    }

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

    // Initialize global parameters
    if (!initialize_global_parameters()) {
        std::cerr << "Failed to initialize time parameters." << std::endl;
        return false;
    }

    // Create the quad
    if (!initialize_quad()) {
        std::cerr << "Failed to create quad." << std::endl;
        return false;
    }

    if (m_render_outputs.size() != 0) {
        set_lead_output(0);
    }

    // Initialize the textures with data
    render();

    m_initialized = true;
    return true;
}

void AudioRenderer::render()
{
    // This replaces the old render() and is called by the event loop
    if (!m_initialized) return;

    m_render_graph->bind();

    // Set the time for the frame
    auto time_param = find_global_parameter("global_time");
    time_param->set_value(m_frame_count);

    glBindVertexArray(m_VAO);

    // render the global parameters
    for (auto& param : m_global_parameters) {
        param->render();
    }

    m_render_graph->render(m_frame_count++);

    // Push to output buffers
    push_to_output_buffers(m_render_graph->get_output_render_stage()->get_output_buffer_data());

    // Unbind everything
    glBindVertexArray(0);
}

AudioRenderer::~AudioRenderer()
{
    // Stop the loop
    m_initialized = false;

    if (m_gl_context) {
        SDL_GL_DeleteContext(m_gl_context);
        m_gl_context = nullptr;
    }

    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    // Delete the vertex array and buffer
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);

    m_render_outputs.clear();
    m_global_parameters.clear();
    m_render_graph.reset();
    m_initialized = false;
    m_frame_count = 0;
    m_lead_output = nullptr;
}

void AudioRenderer::push_to_output_buffers(const float * data)
{
    for (auto& output : m_render_outputs) {
        output->push(data);
    }
}

bool AudioRenderer::is_ready() const {

    // Check if there is outputs to push to 
    if (m_render_outputs.size() == 0) {
        std::cerr << "Error: No outputs to push to." << std::endl;
        return false;
    }

    return m_lead_output->is_ready();
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