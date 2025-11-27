#pragma once
#ifndef AUDIO_RENDER_STAGE_H
#define AUDIO_RENDER_STAGE_H

#include <iostream>
#include <vector>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "audio_core/audio_parameter.h"
#include "utilities/shader_program.h"
#include "audio_core/audio_control.h"
#include "audio_render_stage_plugins/audio_render_stage_plugin.h"

// TODO: Make this a setting global 
#define MAX_TEXTURE_SIZE 4096

class AudioRenderGraph;
class AudioParameter;

class AudioRenderStage {
public:
    friend class AudioRenderGraph;
    friend class AudioRenderStageHistory;

    // Constructor
    static const std::vector<std::string> default_frag_shader_imports;
    static const std::vector<std::string> default_vert_shader_imports;

    // Constructor (auto-generated name)
    AudioRenderStage(const unsigned int frames_per_buffer,
                     const unsigned int sample_rate,
                     const unsigned int num_channels,
                     const std::string& fragment_shader_path = "build/shaders/render_stage_frag.glsl",
                     const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports,
                     const std::string& vertex_shader_path = "build/shaders/render_stage_vert.glsl",
                     const std::vector<std::string> & vert_shader_imports = default_vert_shader_imports);

    // Constructor with explicit name
    AudioRenderStage(const std::string & stage_name,
                     const unsigned int frames_per_buffer,
                     const unsigned int sample_rate,
                     const unsigned int num_channels,
                     const std::string& fragment_shader_path = "build/shaders/render_stage_frag.glsl",
                     const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports,
                     const std::string& vertex_shader_path = "build/shaders/render_stage_vert.glsl",
                     const std::vector<std::string> & vert_shader_imports = default_vert_shader_imports);

    // Constructor that takes fragment shader source as string instead of file path
    AudioRenderStage(const unsigned int frames_per_buffer,
                     const unsigned int sample_rate,
                     const unsigned int num_channels,
                     const std::string& fragment_shader_source,
                     bool use_shader_string, // Dummy parameter to differentiate constructors
                     const std::vector<std::string> & frag_shader_imports = default_frag_shader_imports,
                     const std::string& vertex_shader_path = "build/shaders/render_stage_vert.glsl",
                     const std::vector<std::string> & vert_shader_imports = default_vert_shader_imports);

    // String-based constructor with explicit name
    AudioRenderStage(const std::string & stage_name,
                     const unsigned int frames_per_buffer,
                     const unsigned int sample_rate,
                     const unsigned int num_channels,
                     const std::string& fragment_shader_source,
                     bool use_shader_string, // Dummy parameter to differentiate constructors
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
     * @brief Remove a parameter from the audio parameter list
     * 
     * This function removes a parameter from the audio parameter list
     * 
     * @param name The name of the parameter to remove
     * @return True if the parameter is successfully removed, false otherwise.
     */
    bool remove_parameter(const std::string & name);

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

    const std::unordered_set<AudioRenderStage *> & get_input_connections() const {
        return m_connected_stream_render_stages;
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

    std::vector<std::shared_ptr<AudioControlBase>>& get_controls() {
        return m_controls;
    }

    const std::string & get_name() const {
        return name;
    }

    /**
     * @brief Register a plugin with this render stage
     * 
     * This function registers a plugin, automatically:
     * - Collects shader imports from the plugin
     * - Creates parameters with correct texture counts
     * - Adds all parameters to the render stage
     * 
     * @param plugin The plugin to register (must remain valid for the lifetime of the render stage)
     * @return True if registration is successful, false otherwise
     */
    bool register_plugin(AudioRenderStagePlugin* plugin);

    static const std::string get_shader_source(const std::string & file_path);
    static const std::string combine_shader_source(const std::vector<std::string> & import_paths, const std::string & shader_path);
    static const std::string combine_shader_source_with_string(const std::vector<std::string> & import_paths, const std::string & shader_source);

    const unsigned int gid;    
    const std::string name;

    // TODO: Think of way to make this static
    std::string m_vertex_shader_source;
    std::string m_fragment_shader_source;
    const std::string m_vertex_shader_path;
    const std::string m_fragment_shader_path;
    bool m_uses_shader_string;
    std::string m_fragment_shader_source_string; // Store original shader string for rebuilding
    std::vector<std::string> m_initial_frag_shader_imports; // Store initial imports for rebuilding
    std::vector<std::string> m_initial_vert_shader_imports; // Store initial imports for rebuilding

    // Settings
    const unsigned int frames_per_buffer;
    const unsigned int sample_rate;
    const unsigned int num_channels;

protected:

    /**
     * @brief Process linked parameters together
     * 
     * @return True if success
     */
    virtual bool bind();

    /**
     * @brief Unbind the render stage
     * 
     * This function is responsible for unbinding the render stage.
     */
    virtual bool unbind();

    /**
     * @brief Render the stage.
     * 
     * This function is responsible for rendering the stage and all parameters
     */
    virtual void render(const unsigned int time);

    // Time
    unsigned int m_time = std::numeric_limits<unsigned int>::max(); // Start it at max int value to ensure it is updated on first render

    // Initialized
    bool m_initialized = false;

    GLuint m_active_texture_count = 0;
    GLuint m_color_attachment_count = 0;
    

    virtual const std::vector<AudioParameter *> get_output_interface();
    virtual bool release_output_interface(AudioRenderStage * next_stage);
    virtual const std::vector<AudioParameter *> get_stream_interface();
    virtual bool release_stream_interface(AudioRenderStage * prev_stage);

    // Shader source
    std::unique_ptr<AudioShaderProgram> m_shader_program;

    // Framebuffer for the stage if it involves outputs
    GLuint m_framebuffer;

    // Parameters
    std::unordered_map<std::string, std::unique_ptr<AudioParameter>> m_parameters;
    std::vector<AudioParameter *> m_input_parameters;
    std::vector<AudioParameter *> m_output_parameters;
    std::vector<GLenum> m_draw_buffers;
    std::unordered_set<AudioRenderStage *> m_connected_output_render_stages;
    std::unordered_set<AudioRenderStage *> m_connected_stream_render_stages;

    // Controls for this render stage
    std::vector<std::shared_ptr<AudioControlBase>> m_controls;

    // Registered plugins
    std::vector<AudioRenderStagePlugin*> m_plugins;

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

protected:
    unsigned int generate_id() {
        static unsigned int id = 1; // Render stage GIDs start at 1
        return id++;
    }

private:

    void print_input_textures();

    void print_output_textures();

    void clear_input_textures();

    void clear_output_textures();

    // Shared constructor logic to set up parameters and shader program
    void setup_default_parameters();

    /**
     * @brief Rebuild shader sources and create shader program with all registered plugin imports
     * 
     * This function collects shader imports from all registered plugins,
     * rebuilds the vertex and fragment shader sources, and creates the shader program.
     */
    void rebuild_shader_sources();
};

#endif // AUDIO_RENDER_STAGE_H