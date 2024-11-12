#pragma once
#ifndef AUDIO_RENDER_STAGE_H
#define AUDIO_RENDER_STAGE_H

#include <vector>
#include <unordered_map>
#include <GL/glew.h>

#include "audio_parameter/audio_parameter.h"

class AudioRenderer;
class AudioParameter;

class AudioRenderStage {
public:
    friend class AudioRenderer;
    // Constructor
    AudioRenderStage(const unsigned int frames_per_buffer,
                     const unsigned int sample_rate,
                     const unsigned int num_channels);

    // Destructor
    virtual ~AudioRenderStage(); // Make destructor virtual

    /** 
     * @brief Add a parameter to the audio parameter list
     * 
     * This function adds a parameter to the audio parameter list
     * 
     * @param parameter The parameter to add
     * @return True if the parameter is successfully added, false otherwise.
     */
    bool add_parameter(AudioParameter * parameter);

    /**
     * @brief Find a parameter by name
     * 
     * This function finds a parameter by name
     * 
     * @param name The name of the parameter to find
     * @return The parameter if found, nullptr otherwise.
     */
    AudioParameter * find_parameter(const char * name) const;

    // Getters
    GLuint get_shader_program() const {
        return m_shader_program;
    }

    GLuint get_framebuffer() const {
        return m_framebuffer;
    }

    virtual GLchar const * get_fragment_source() const {
        return R"glsl(
            #version 300 es
            precision highp float;

            in vec2 TexCoord;

            uniform sampler2D stream_audio_texture;

            layout(std140) uniform global_time {
                int global_time_val;
            };

            out vec4 output_audio_texture;

            void main() {
                output_audio_texture = texture(stream_audio_texture, TexCoord);
            }
        )glsl";
    }

    const unsigned int gid;    

protected:
    // Settings
    const unsigned int m_frames_per_buffer;
    const unsigned int m_sample_rate;
    const unsigned int m_num_channels;

    GLuint m_active_texture_count = 0;
    GLuint m_color_attachment_count = 0;
    
    // Link to the renderer
    const AudioRenderer * m_renderer_link;

    /**
     * @brief Initializes the audio render stage.
     * 
     * This function is responsible for initializing the audio render stage.
     * 
     * @return True if initialization is successful, false otherwise.
     */
    virtual bool initialize_shader_stage();

    /**
     * @brief Process linked parameters together
     * 
     * @return True if success
     */
    virtual bool bind_shader_stage();

    /**
     * @brief Render the stage.
     * 
     * This function is responsible for rendering the stage and all parameters
     */
    virtual void render_render_stage();

    // Shader source
    GLuint m_shader_program = 0; // Keeps a copy of the shader program associated with the stage

    // Framebuffer for the stage if it involves outputs
    GLuint m_framebuffer;

    // Parameters
    std::vector<std::unique_ptr<AudioParameter>> m_parameters;

    // Link to the renderer
    void link_renderer(const AudioRenderer * renderer) {
        m_renderer_link = renderer;
    }

private:

    /**
     * @brief Compile the shader program.
     * 
     * This function is responsible for compiling the shader program.
     */
    bool initialize_shader_program();

    /**
     * @brief Compile parameters
     * 
     * This function prepared the framebuffers and textures for the parameters
     */
    bool initialize_stage_parameters();

    /**
     * @brief Initialize the framebuffer.
     * 
     * This function is responsible for initializing the framebuffer.
     */
    bool initialize_framebuffer();


    unsigned int generate_id() {
        static unsigned int id = 0;
        return id++;
    }
};

#endif // AUDIO_RENDER_STAGE_H