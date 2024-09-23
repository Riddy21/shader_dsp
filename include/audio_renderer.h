#pragma once
#ifndef AUDIO_RENDERER_H
#define AUDIO_RENDERER_H

#include <vector>
#include <memory>
#include <mutex>
#include <GL/glew.h>
#include <GL/glut.h>

#include "audio_buffer.h"
#include "audio_render_stage.h"

class AudioRenderStage;

class AudioParameter;

// TODO: Make this class so that the generator can run
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

    /**
     * @brief Initializes the audio renderer with the specified buffer size and number of channels.
     * 
     * @param buffer_size The size of the audio data buffer.
     * @param num_channels The number of audio channels.
     * @return True if initialization is successful, false otherwise.
     */
    bool init(const unsigned int buffer_size, const unsigned int sample_rate, const unsigned int num_channels);

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

    /**
     * @brief Main loop for the audio renderer.
     */
    static void main_loop();

    static void iterate() {
        render_callback();
        draw_callback(0);
    }

    /**
     * @brief Adds a render stage to the audio renderer.
     * 
     * @param render_stage The render stage to be added.
     * @return True if the render stage is successfully added, false otherwise.
     */
    bool add_render_stage(AudioRenderStage & render_stage);

    /**
     * @brief Adds a keyboard down callback to the audio renderer.
     * 
     * @param callback The callback function to be added.
     * @return True if the callback is successfully added, false otherwise.
     */
    bool add_keyboard_down_callback(void (*callback)(unsigned char, int, int)){
        glutKeyboardFunc(callback);
        return true;
    }

    /**
     * @brief Adds a keyboard up callback to the audio renderer.
     * 
     * @param callback The callback function to be added.
     * @return True if the callback is successfully added, false otherwise.
     */
    bool add_keyboard_up_callback(void (*callback)(unsigned char, int, int)){
        glutKeyboardUpFunc(callback);
        return true;
    }

    /**
     * @brief Returns a new output buffer for the audio renderer
     * 
     * @return The output buffer data.
     */
    AudioBuffer * get_new_output_buffer();

    AudioRenderStage * get_render_stage(const unsigned int index) {
        return m_render_stages[index].get();
    }

    // Vertex Source code
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
    // Private member functions
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

    /**
     * @brief Renders one stage of the audio data through OpenGL
     */
    void draw(unsigned int frame);

    /**
     * @brief Render a frame
     */
    void render();

    /**
     * @brief Calculates the frame rate of the audio renderer
     */
    void calculate_frame_rate();

    /**
     * @brief Static callback function for drawing the audio data
     * 
     * @param arg The argument to pass to the draw function
     */
    static void draw_callback(int arg) {
        instance->draw(arg);
    }

    /**
     * @brief Static callback function for rendering
     */
    static void render_callback() {
        instance->render();
    }

    /**
     * @brief Static callback function for displaying
     */
    static void display_callback() {
        glutSwapBuffers(); // unlock framerate
        glutPostRedisplay();
    }

    /**
     * @brief Push data to all output buffers
     * 
     * @param data The data to push
     * @param size The size of the data
     */
    void push_data_to_all_output_buffers(const float * data, const unsigned int size);

    // Private member variables
    GLuint m_VAO; // Vertex Array For holding vertex attribute configurations
    GLuint m_VBO; // Vertex Array buffer For holding vertex data
    GLuint m_PBO; // Pixel buffer object for inputting and outputting to screen

    unsigned int m_num_stages; // Number of audio buffers
    unsigned int m_buffer_size; // Size of audio data
    unsigned int m_num_channels; // Number of audio channels
    unsigned int m_sample_rate; // Sample rate of audio data

    unsigned int m_frame_count = 0; // Frame count for calculating frame rate

    // Marking initialized
    bool m_initialized = false;

    // Mutex for locking the audio data
    std::mutex m_audio_data_mutex;

    // buffers for audio data
    std::vector<std::unique_ptr<AudioBuffer>> m_output_buffers = std::vector<std::unique_ptr<AudioBuffer>>(); // Output buffers

    // Simplify this into one struct
    // FIXME: Convert this into a unique pointer
    std::vector<std::shared_ptr<AudioRenderStage>> m_render_stages;

    AudioParameter * m_frame_time_parameter = nullptr;

};

#endif // AUDIO_RENDERER_H