#pragma once
#ifndef AUDIO_RENDERER_H
#define AUDIO_RENDERER_H

#include <vector>
#include <memory>
#include <GL/glew.h>

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
public:
    /**
     * @brief Returns the singleton instance of AudioRenderer.
     * 
     * @return The singleton instance of AudioRenderer.
     */
    static AudioRenderer& get_instance() {
        static AudioRenderer instance;
        return instance;
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
    bool init(const unsigned int buffer_size, const unsigned int num_channels);

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

    /**
     * @brief Adds a render stage to the audio renderer.
     * 
     * @param render_stage The render stage to be added.
     * @return True if the render stage is successfully added, false otherwise.
     */
    bool add_render_stage(AudioRenderStage & render_stage);

private:
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
        AudioRenderer * renderer = (AudioRenderer *)arg;
        renderer->render(arg);
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
    GLuint PBO[2]; // Pixel buffer object for inputting and outputting to screen
    GLuint FBO[2]; // Frame buffer object for swapping frames during shader processing
    GLuint audio_texture[3]; // Audio texture for holding audio data

    unsigned int num_stages; // Number of audio buffers
    unsigned int buffer_size; // Size of audio data
    unsigned int num_channels; // Number of audio channels

    // buffers for audio data
    std::shared_ptr<std::vector<float>> input_buffer_data;
    std::vector<float> output_buffer_data;

    // Simplify this into one struct
    std::vector<AudioRenderStage> render_stages;

    // Vertex Source code
    const GLchar* vertex_source = R"glsl(
        #version 330 es
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
};

#endif // AUDIO_RENDERER_H