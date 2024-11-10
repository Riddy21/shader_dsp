#pragma once
#ifndef AUDIO_RENDERER_H
#define AUDIO_RENDERER_H

#include <vector>
#include <memory>
#include <mutex>
#include <GL/glew.h>
#include <GL/glut.h>
#include <atomic>

#include "audio_buffer.h"
#include "audio_render_stage.h"
#include "audio_output.h"
#include "audio_parameter.h"

class AudioRenderStage;
class AudioParameter;

/**
 * @class AudioRenderer
 * @brief The AudioRenderer class is responsible for rendering audio data.
 * 
 * The AudioRenderer class provides functionality to initialize and terminate the audio renderer,
 * as well as holding audio texture output and managing audio buffer size and number of channels.
 */
class AudioRenderer {
public:
    /**
     * @brief Returns the singleton instance of AudioRenderer.
     * 
     * @return The singleton instance of AudioRenderer.
     */
    static AudioRenderer& get_instance() {
        if (!instance) {
            instance = new AudioRenderer();
        }
        return *instance;
    }

    // Delete copy constructor and assignment operator
    AudioRenderer(AudioRenderer const&) = delete;
    void operator=(AudioRenderer const&) = delete;

// -------------Main Loop Functions----------------
    /**
     * @brief Initializes the audio renderer with the specified buffer size, sample rate, and number of channels.
     * 
     * @param buffer_size The size of the audio data buffer.
     * @param sample_rate The sample rate of the audio data.
     * @param num_channels The number of audio channels.
     * @return True if initialization is successful, false otherwise.
     */
    bool initialize(const unsigned int buffer_size, const unsigned int sample_rate, const unsigned int num_channels);

    /**
     * @brief Checks if the audio renderer is initialized.
     * 
     * @return True if the audio renderer is initialized, false otherwise.
     */
    bool is_initialized() {
        return m_initialized;
    }

    /**
     * @brief Starts the main loop of the audio renderer.
     */
    void start_main_loop();

    /**
     * @brief Terminates the audio renderer.
     * 
     * @return True if termination is successful, false otherwise.
     */
    bool terminate();

    /**
     * @brief Cleans up the audio renderer.
     * 
     * @return True if cleanup is successful, false otherwise.
     */
    bool cleanup();

    static void iterate() {
        render_callback();
    }

// -------------Add Functions----------------
    /**
     * @brief Adds a render stage to the audio renderer.
     * 
     * @param render_stage The render stage to be added.
     * @return True if the render stage is successfully added, false otherwise.
     */
    bool add_render_stage(AudioRenderStage * render_stage);

    /**
     * @brief Adds an output link to the audio renderer.
     * 
     * @param output_link The output link to be added.
     * @return True if the output link is successfully added, false otherwise.
     */
    bool add_render_output(AudioOutput * output_link);

// -------------Setters----------------
    /**
     * @brief The lead output is the output device that sets the timing for the audio renderer.
     *        This function sets the lead output device.
     * 
     * @param gid The global ID of the lead output device.
     */
    void set_lead_output(const unsigned int gid) {
        m_lead_output = m_render_outputs[gid].get();
    }

// -------------Getters----------------

    /**
     * @brief Finds a render stage by its global ID.
     * 
     * @param gid The global ID of the render stage.
     * @return The render stage if found, nullptr otherwise.
     */
    AudioRenderStage * find_render_stage(const unsigned int gid);

    AudioOutput * find_render_output(const unsigned int gid);

    // TODO: Put this in a seperate file
    /**
     * @brief Returns the vertex shader source code.
     * 
     * @return The vertex shader source code.
     */
    const GLchar* get_vertex_source() const {
        return R"glsl(
            #version 300 es
            precision highp float;
            layout (location = 0) in vec2 aPos;
            layout (location = 1) in vec2 aTexCoord;
            out vec2 TexCoord;
            void main()
            {
                gl_Position = vec4(aPos, 0.0, 1.0);
                TexCoord = aTexCoord;
            }
        )glsl";
    }


private:
    static AudioRenderer * instance;

// -------------Main Loop Functions----------------
    /**
     * @brief Constructs an AudioRenderer object.
     * 
     * Private constructor to enforce singleton pattern.
     */
    AudioRenderer() {};

    /**
     * @brief Destroys the AudioRenderer object.
     * 
     * Private destructor to enforce singleton pattern.
     */
    ~AudioRenderer();

// -------------Render Functions----------------
    /**
     * @brief Renders a frame.
     */
    void render();

// -------------Helper Functions----------------
    /**
     * @brief Sets the time parameter for all render stages.
     * 
     * @param time The time value to set.
     */
    void set_all_time_parameters(const unsigned int time);

    /**
     * @brief Calculates the frame rate of the audio renderer.
     */
    void calculate_frame_rate();

    /**
     * @brief Pushes data to all output buffers.
     * 
     * @param data The data to push.
     */
    void push_to_output_buffers(const float * data);


// -------------Callback Functions----------------
    /**
     * @brief Static callback function for rendering.
     */
    static void render_callback() {
        instance->render();
    }

    /**
     * @brief Static callback function for displaying.
     */
    static void display_callback() {
        glutSwapBuffers(); // unlock framerate
        glutPostRedisplay();
    }

// -------------Initialization Functions----------------
    /**
     * @brief Initializes the time parameters for the render stages.
     * 
     * @return True if initialization is successful, false otherwise.
     */
    bool initialize_time_parameters();

    /**
     * @brief Initializes the OpenGL context.
     * 
     * @param window_width The width of the window.
     * @param window_height The height of the window.
     * @return True if initialization is successful, false otherwise.
     */
    bool initialize_glut(unsigned int window_width, unsigned int window_height);

    /**
     * @brief Initializes the quad for rendering.
     * 
     * @param VAO The Vertex Array Object.
     * @param VBO The Vertex Buffer Object.
     * @param PBO The Pixel Buffer Object.
     * @param num_channels The number of audio channels.
     * @param buffer_size The size of the audio data buffer.
     * @return True if initialization is successful, false otherwise.
     */
    bool initialize_quad(GLuint &VAO, GLuint &VBO, GLuint &PBO, const unsigned int num_channels, const unsigned int buffer_size);

    GLuint m_VAO; // Vertex Array Object for holding vertex attribute configurations
    GLuint m_VBO; // Vertex Buffer Object for holding vertex data
    GLuint m_PBO; // Pixel Buffer Object for inputting and outputting to screen

    unsigned int m_num_stages; // Number of audio buffers
    unsigned int m_buffer_size; // Size of audio data
    unsigned int m_num_channels; // Number of audio channels
    unsigned int m_sample_rate; // Sample rate of audio data

    unsigned int m_frame_count = 0; // Frame count for calculating frame rate
    AudioOutput * m_lead_output = nullptr; // Lead output for frame rate calculation

    std::atomic<bool> m_running; // Flag to control the loop

    bool m_initialized = false; // Flag to mark initialization

    std::vector<std::unique_ptr<AudioOutput>> m_render_outputs; // Render outputs
    std::vector<std::unique_ptr<AudioRenderStage>> m_render_stages; // Render stages
    std::vector<AudioParameter *> m_frame_time_parameters; // Time parameters for render stages

};

#endif // AUDIO_RENDERER_H