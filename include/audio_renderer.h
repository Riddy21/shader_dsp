#pragma once
#ifndef AUDIO_RENDERER_H
#define AUDIO_RENDERER_H

#include <vector>
#include <memory>
#include <mutex>
#include <GL/glew.h>
#include <GL/glut.h>

#include "audio_render_stage.h"

// TODO: Make this class so that the generator can run
/**
 * @class AudioRenderer
 * @brief The AudioRenderer class is responsible for rendering audio data.
 * 
 * The AudioRenderer class provides functionality to initialize and terminate the audio renderer,
 * as well as holding audio texture output and managing audio buffer size and number of channels.
 */
class AudioRenderer {
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
    void render(int value);

    static void render_callback(int arg) {
        instance->render(arg);
    }

    static void display_callback() {
        glutSwapBuffers(); // unlock framerate
        glutPostRedisplay();
    }

    /**
     * @brief Creates a shader program
     * 
     * @param vertex_source The vertex shader source code
     * @param fragment_source The fragment shader source code
     * 
     * @return The shader program
     */
    static GLuint compile_shaders(const GLchar* vertex_source, const GLchar* fragment_source);

    // Private member variables
    GLuint VAO; // Vertex Array For holding vertex attribute configurations
    GLuint VBO; // Vertex Array buffer For holding vertex data
    GLuint PBO; // Pixel buffer object for inputting and outputting to screen
    GLuint FBO[3]; // Frame buffer object for swapping frames during shader processing
    GLuint audio_texture[3]; // Audio texture for holding audio data

    unsigned long frame_count = 0; // Frame count for debugging

    unsigned int num_stages; // Number of audio buffers
    unsigned int buffer_size; // Size of audio data
    unsigned int num_channels; // Number of audio channels
    unsigned int sample_rate; // Sample rate of audio data

    // buffers for audio data
    std::vector<float> input_buffer_data; // TODO: This should be something else in the future
    std::vector<float> output_buffer_data;

    // Mutex for locking the audio data
    std::mutex audio_mutex = std::mutex();

    // Simplify this into one struct
    std::vector<AudioRenderStage * > render_stages; // FIXME: Make this a shared pointer

    // Vertex Source code
    const GLchar* vertex_source = R"glsl(
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
        render_callback(0);
    }

    /**
     * @brief Adds a render stage to the audio renderer.
     * 
     * @param render_stage The render stage to be added.
     * @return True if the render stage is successfully added, false otherwise.
     */
    bool add_render_stage(AudioRenderStage & render_stage);

    /**
     * @brief Returns the output buffer data.
     * 
     * @return The output buffer data.
     */
    const std::vector<float> & get_output_buffer_data() {
        return output_buffer_data;
    }

    /**
     * @brief Returns the audio mutex.
     * 
     * @return The audio mutex.
     */
    std::mutex & get_audio_mutex() {
        return audio_mutex;
    }
};

#endif // AUDIO_RENDERER_H