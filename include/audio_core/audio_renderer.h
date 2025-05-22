#pragma once
#ifndef AUDIO_RENDERER_H
#define AUDIO_RENDERER_H

#include <vector>
#include <memory>
#include <mutex>
#include <SDL2/SDL.h>
#include <atomic>
#include <thread>

#include "audio_core/audio_render_stage.h"
#include "audio_output/audio_output.h"
#include "audio_core/audio_parameter.h"
#include "audio_core/audio_render_graph.h"
#include "engine/event_loop.h"
#include "engine/renderable_item.h"

/**
 * @class AudioRenderer
 * @brief The AudioRenderer class is responsible for rendering audio data.
 * 
 * The AudioRenderer class provides functionality to initialize and terminate the audio renderer,
 * as well as holding audio texture output and managing audio buffer size and number of channels.
 */
class AudioRenderer : public IRenderableEntity {
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

    // IEventLoopItem interface
    bool is_ready() override;

    void render() override;

    void present() override;

    void activate_render_context() override; // Add this line

// Loop Control
    void pause() {
        m_paused = true;
    }

    void increment() {
        m_increment = true;
    }

    void resume() {
        m_paused = false;
    }

// -------------Add Functions----------------
    /**
     * @brief Adds an output link to the audio renderer.
     * 
     * @param output_link The output link to be added.
     * @return True if the output link is successfully added, false otherwise.
     */
    bool add_render_output(AudioOutput * output_link);

    /**
     * @brief Adds a global parameter to the audio renderer.
     * 
     * @param parameter The global parameter to be added.
     * @return True if the global parameter is successfully added, false otherwise.
     */
    bool add_global_parameter(AudioParameter * parameter);

    /**
     * @brief Adds a render stage tree to the audio renderer.
     * 
     * @param render_stages The render stages to be added.
     * @return True if the render stages are successfully added, false otherwise.
     */
    bool add_render_graph(AudioRenderGraph * render_graph);

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
     * @brief Checks if the audio renderer is initialized.
     * 
     * @return True if the audio renderer is initialized, false otherwise.
     */
    bool is_initialized() {
        return m_initialized;
    }

    /**
     * @brief Returns the render graph of the audio renderer.
     * 
     * @return The render graph of the audio renderer.
     */
    AudioRenderGraph * get_render_graph() {
        return m_render_graph.get();
    }

    /**
     * @brief Finds an output link by its global ID.
     * 
     * @param gid The global ID of the output link.
     * @return The output link if found, nullptr otherwise.
     */
    AudioOutput * find_render_output(const unsigned int gid);

    /**
     * @brief Finds a global parameter by its name.
     * 
     * @param name The name of the global parameter.
     * @return The global parameter if found, nullptr otherwise.
     */
    AudioParameter * find_global_parameter(const std::string name) const;


private:
    static AudioRenderer * instance;

// -------------Main Loop Functions----------------
    /**
     * @brief Constructs an AudioRenderer object.
     * 
     * Private constructor to enforce singleton pattern.
     */
    AudioRenderer();

    /**
     * @brief Destroys the AudioRenderer object.
     * 
     * Private destructor to enforce singleton pattern.
     */
    ~AudioRenderer();

// -------------Helper Functions----------------

    /**
     * @brief Pushes data to all output buffers.
     * 
     * @param data The data to push.
     */
    void push_to_output_buffers(const float * data);


// -------------Initialization Functions----------------
    /**
     * @brief Initializes the time parameters for the render stages.
     * 
     * @return True if initialization is successful, false otherwise.
     */
    bool initialize_global_parameters();

    /**
     * @brief Initializes the SDL context.
     * 
     * @param window_width The width of the window.
     * @param window_height The height of the window.
     * @return True if initialization is successful, false otherwise.
     */
    bool initialize_sdl(unsigned int window_width, unsigned int window_height);

    /**
     * @brief Initializes the quad for rendering.
     * 
     * @param VAO The Vertex Array Object.
     * @param VBO The Vertex Buffer Object.
     * @param num_channels The number of audio channels.
     * @param buffer_size The size of the audio data buffer.
     * @return True if initialization is successful, false otherwise.
     */
    bool initialize_quad();

    GLuint m_VAO; // Vertex Array Object for holding vertex attribute configurations
    GLuint m_VBO; // Vertex Buffer Object for holding vertex data

    unsigned int m_buffer_size; // Size of audio data
    unsigned int m_num_channels; // Number of audio channels
    unsigned int m_sample_rate; // Sample rate of audio data

    unsigned int m_frame_count = 0; // Frame count for calculating frame rate
    AudioOutput * m_lead_output = nullptr; // Lead output for frame rate calculation

    bool m_initialized = false; // Flag to mark initialization
    bool m_paused = false; // Flag to mark pause state
    bool m_increment = false; // Flag to mark increment state

    std::vector<std::unique_ptr<AudioOutput>> m_render_outputs; // Render outputs
    std::vector<std::unique_ptr<AudioParameter>> m_global_parameters; // Parameters for render stages
    std::unique_ptr<AudioRenderGraph> m_render_graph; // Render graph

    SDL_Window* m_window = nullptr; // SDL window pointer
    SDL_GLContext m_context = nullptr; // SDL OpenGL context
};

#endif // AUDIO_RENDERER_H