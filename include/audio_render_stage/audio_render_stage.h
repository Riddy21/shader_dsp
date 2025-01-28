#pragma once
#ifndef AUDIO_RENDER_STAGE_H
#define AUDIO_RENDER_STAGE_H

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <GL/glew.h>

#include "audio_parameter/audio_parameter.h"
#include "audio_core/audio_shader_program.h"

#define MAX_TEXTURE_SIZE 4096

class AudioRenderGraph;
class AudioParameter;

class AudioRenderStage {
public:
    friend class AudioRenderGraph;

    // Constructor
    static const std::vector<std::string> default_frag_shader_imports;
    static const std::vector<std::string> default_vert_shader_imports;

    AudioRenderStage(const unsigned int frames_per_buffer,
                     const unsigned int sample_rate,
                     const unsigned int num_channels,
                     const std::string& fragment_shader_path = "build/shaders/render_stage_frag.glsl",
                     const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports,
                     const std::string& vertex_shader_path = "build/shaders/render_stage_vert.glsl",
                     const std::vector<std::string> & vert_shader_imports = default_vert_shader_imports);

    // Destructor
    virtual ~AudioRenderStage();

    /**
     * @brief Initializes the audio render stage.
     * 
     * This function is responsible for initializing the audio render stage.
     * 
     * @return True if initialization is successful, false otherwise.
     */
    virtual bool initialize();

    // Parameter Manipulation
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
    AudioParameter * find_parameter(const std::string name) const;

    // Render stage manipulation
    virtual bool connect_render_stage(AudioRenderStage * next_stage);

    virtual bool disconnect_render_stage(AudioRenderStage * next_stage);

    virtual bool disconnect_render_stage();


    // Getters
    const std::vector<AudioParameter *> & get_input_parameters() const {
        return m_input_parameters;
    }

    const std::vector<AudioParameter *> & get_output_parameters() const {
        return m_output_parameters;
    }

    GLuint get_shader_program() const {
        return m_shader_program->get_program();
    }

    GLuint get_framebuffer() const {
        return m_framebuffer;
    }

    bool is_initialized() const {
        return m_initialized;
    }

    static const std::string get_shader_source(const std::string & file_path);
    static const std::string combine_shader_source(const std::vector<std::string> & import_paths, const std::string & shader_path);

    const unsigned int gid;    

    // TODO: Think of way to make this static
    const std::string m_vertex_shader_source;
    const std::string m_fragment_shader_source;

protected:

    /**
     * @brief Process linked parameters together
     * 
     * @return True if success
     */
    virtual bool bind();

    /**
     * @brief Render the stage.
     * 
     * This function is responsible for rendering the stage and all parameters
     */
    virtual void render(const unsigned int time);

    // Time
    unsigned int m_time = 0;

    // Initialized
    bool m_initialized = false;

    // Settings
    const unsigned int m_frames_per_buffer;
    const unsigned int m_sample_rate;
    const unsigned int m_num_channels;

    GLuint m_active_texture_count = 0;
    GLuint m_color_attachment_count = 0;
    

    virtual const std::vector<AudioParameter *> get_output_interface();
    virtual bool release_output_interface(AudioRenderStage * next_stage);
    virtual const std::vector<AudioParameter *> get_stream_interface();
    virtual bool release_stream_interface(AudioRenderStage * prev_stage) {return true;};

    // Shader source
    std::unique_ptr<AudioShaderProgram> m_shader_program;

    // Framebuffer for the stage if it involves outputs
    GLuint m_framebuffer;

    // Parameters
    std::unordered_map<std::string, std::unique_ptr<AudioParameter>> m_parameters;
    std::vector<AudioParameter *> m_input_parameters;
    std::vector<AudioParameter *> m_output_parameters;
    std::unordered_set<AudioRenderStage *> m_connected_output_render_stages;
    std::unordered_set<AudioRenderStage *> m_connected_stream_render_stages;

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
        static unsigned int id = 1; // Render stage GIDs start at 1
        return id++;
    }
};

#endif // AUDIO_RENDER_STAGE_H