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

    // Add itself to the event loop
    auto & event_loop = EventLoop::get_instance();
    event_loop.add_loop_item(this); // Register this audio renderer instance with the event loop
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

// Now uses parent class initialize_sdl method

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

bool AudioRenderer::initialize(const unsigned int buffer_size, const unsigned int sample_rate, const unsigned int num_channels) {
    if (m_initialized) {
        std::cerr << "Error: Audio renderer already initialized." << std::endl;
        return false;
    }

    // TODO: Move these to the constructor
    this->m_buffer_size = buffer_size;
    this->m_num_channels = num_channels;
    this->m_sample_rate = sample_rate;

    // Initialize SDL2 using the parent class method
    if (!initialize_sdl(buffer_size, num_channels, "Audio Processing", SDL_WINDOW_OPENGL, false)) {
        return false;
    }

    activate_render_context(); // Set the current context for this thread

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

    // Initialize the textures with data
    render();

    m_initialized = true;
    return true;
}

void AudioRenderer::render()
{
    IRenderableEntity::render();

    // No need to call activate_render_context() here, event loop will do it
    if (!m_initialized) return;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    m_render_graph->bind();

    // Set the time for the frame
    auto time_param = find_global_parameter("global_time");
    time_param->set_value(m_frame_count);

    glBindVertexArray(m_VAO);

    // render the global parameters
    for (auto& param : m_global_parameters) {
        param->render();
    }

    m_render_graph->render(m_frame_count);

    // Unbind everything
    glBindVertexArray(0);
}

void AudioRenderer::present()
{
    IRenderableEntity::present();
    // No need to call activate_render_context() here, event loop will do it
    push_to_output_buffers(m_render_graph->get_output_render_stage()->get_output_buffer_data().data());
    m_frame_count++;
}

AudioRenderer::~AudioRenderer()
{
    activate_render_context();

    // Stop the loop
    m_initialized = false;

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
    m_increment = false;
    for (auto& output : m_render_outputs) {
        output->push(data);
    }
}

bool AudioRenderer::is_ready() {

    if (m_increment) {
        return true;
    }

    if (m_paused) {
        return false;
    }

    // Check if there is outputs to push to 
    if (m_render_outputs.size() == 0) {
        std::cerr << "Error: No outputs to push to." << std::endl;
        return false;
    }

    if (m_render_outputs.size() != 0) {
        set_lead_output(0);
    }

    if (!m_lead_output->is_ready()) {
        return false;
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

// Now using the base class implementation of activate_render_context()