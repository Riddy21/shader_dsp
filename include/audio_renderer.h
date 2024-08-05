#pragma once
#ifndef AUDIO_RENDERER_H
#define AUDIO_RENDERER_H

#include <GL/glew.h>

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
     * @brief Constructs an AudioRenderer object with the specified buffer size and number of channels.
     * 
     * @param buffer_size The size of the audio data buffer.
     * @param num_channels The number of audio channels.
     */
    AudioRenderer(const unsigned int buffer_size, const unsigned int num_channels);

    /**
     * @brief Destroys the AudioRenderer object.
     */
    ~AudioRenderer();

    /**
     * @brief Initializes the audio renderer.
     * 
     * @return True if initialization is successful, false otherwise.
     */
    bool init();

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

private:
    // Private member variables
    GLuint VAO; // Vertex Array For holding audio texture output
    GLuint VBO; // Vertex Array buffer For holding audio texture output
    GLuint EBO; // Element buffer object for holding audio texture output
    GLuint PBO[2]; // Pixel buffer object for swaping frames during shader processing
    GLuint FBO[2]; // Frame buffer object for swaping frames during shader processing
    GLuint audio_texture[2]; // Audio texture for holding audio data
    const unsigned int buffer_size; // Size of audio data
    const unsigned int num_channels; // Number of audio channels

};

#endif // AUDIO_RENDERER_H