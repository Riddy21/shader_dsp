#include <iostream>
#include <cstring>
#include <typeinfo>
#include <algorithm>

#include "audio_core/audio_renderer.h"
#include "audio_core/audio_render_stage.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_parameter/audio_texture2d_parameter.h"
#include "audio_render_stage/audio_effect_render_stage.h"
#include "audio_render_stage/audio_multitrack_join_render_stage.h"
#include "audio_render_stage/audio_file_generator_render_stage.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"

const std::vector<std::string> AudioRenderStage::default_frag_shader_imports = {
    "build/shaders/global_settings.glsl",
    "build/shaders/frag_shader_settings.glsl"
};

const std::vector<std::string> AudioRenderStage::default_vert_shader_imports = {
    "build/shaders/global_settings.glsl"
};

AudioRenderStage::AudioRenderStage(const unsigned int frames_per_buffer,
                                   const unsigned int sample_rate,
                                    const unsigned int num_channels,
                                   const std::string & fragment_shader_path,
                                   const std::vector<std::string> & frag_shader_imports,
                                   const std::string & vertex_shader_path,
                                   const std::vector<std::string> & vert_shader_imports)
                                  : gid(generate_id()),
                                    name("RenderStage-" + std::to_string(gid)),
                                    frames_per_buffer(frames_per_buffer),
                                    sample_rate(sample_rate),
                                    num_channels(num_channels),
                                    m_vertex_shader_path(vertex_shader_path),
                                    m_fragment_shader_path(fragment_shader_path),
                                    m_uses_shader_string(false),
                                    m_initial_frag_shader_imports(frag_shader_imports),
                                    m_initial_vert_shader_imports(vert_shader_imports) {
    // Combine shader sources with initial imports
    m_vertex_shader_source = combine_shader_source(vert_shader_imports, vertex_shader_path);
    m_fragment_shader_source = combine_shader_source(frag_shader_imports, fragment_shader_path);
    setup_default_parameters();
}

// Constructor with explicit name
AudioRenderStage::AudioRenderStage(const std::string & stage_name,
                                   const unsigned int frames_per_buffer,
                                   const unsigned int sample_rate,
                                   const unsigned int num_channels,
                                   const std::string & fragment_shader_path,
                                   const std::vector<std::string> & frag_shader_imports,
                                   const std::string & vertex_shader_path,
                                   const std::vector<std::string> & vert_shader_imports)
                                  : gid(generate_id()),
                                    name(stage_name),
                                    frames_per_buffer(frames_per_buffer),
                                    sample_rate(sample_rate),
                                    num_channels(num_channels),
                                    m_vertex_shader_path(vertex_shader_path),
                                    m_fragment_shader_path(fragment_shader_path),
                                    m_uses_shader_string(false),
                                    m_initial_frag_shader_imports(frag_shader_imports),
                                    m_initial_vert_shader_imports(vert_shader_imports) {
    // Combine shader sources with initial imports
    m_vertex_shader_source = combine_shader_source(vert_shader_imports, vertex_shader_path);
    m_fragment_shader_source = combine_shader_source(frag_shader_imports, fragment_shader_path);
    setup_default_parameters();
}

// Constructor that takes fragment shader source as string
AudioRenderStage::AudioRenderStage(const unsigned int frames_per_buffer,
                                   const unsigned int sample_rate,
                                   const unsigned int num_channels,
                                   const std::string & fragment_shader_source,
                                   bool use_shader_string,
                                   const std::vector<std::string> & frag_shader_imports,
                                   const std::string & vertex_shader_path,
                                   const std::vector<std::string> & vert_shader_imports)
                                  : gid(generate_id()),
                                    name("RenderStage-" + std::to_string(gid)),
                                    frames_per_buffer(frames_per_buffer),
                                    sample_rate(sample_rate),
                                    num_channels(num_channels),
                                    m_vertex_shader_path(vertex_shader_path),
                                    m_fragment_shader_path(""), // Empty for string-based shader
                                    m_uses_shader_string(true),
                                    m_fragment_shader_source_string(fragment_shader_source),
                                    m_initial_frag_shader_imports(frag_shader_imports),
                                    m_initial_vert_shader_imports(vert_shader_imports) {
    // Combine shader sources with initial imports
    m_vertex_shader_source = combine_shader_source(vert_shader_imports, vertex_shader_path);
    m_fragment_shader_source = combine_shader_source_with_string(frag_shader_imports, fragment_shader_source);
    setup_default_parameters();
}

// String-based constructor with explicit name
AudioRenderStage::AudioRenderStage(const std::string & stage_name,
                                   const unsigned int frames_per_buffer,
                                   const unsigned int sample_rate,
                                   const unsigned int num_channels,
                                   const std::string & fragment_shader_source,
                                   bool use_shader_string,
                                   const std::vector<std::string> & frag_shader_imports,
                                   const std::string & vertex_shader_path,
                                   const std::vector<std::string> & vert_shader_imports)
                                  : gid(generate_id()),
                                    name(stage_name),
                                    frames_per_buffer(frames_per_buffer),
                                    sample_rate(sample_rate),
                                    num_channels(num_channels),
                                    m_vertex_shader_path(vertex_shader_path),
                                    m_fragment_shader_path(""), // Empty for string-based shader
                                    m_uses_shader_string(true),
                                    m_fragment_shader_source_string(fragment_shader_source),
                                    m_initial_frag_shader_imports(frag_shader_imports),
                                    m_initial_vert_shader_imports(vert_shader_imports) {
    // Combine shader sources with initial imports
    m_vertex_shader_source = combine_shader_source(vert_shader_imports, vertex_shader_path);
    m_fragment_shader_source = combine_shader_source_with_string(frag_shader_imports, fragment_shader_source);
    setup_default_parameters();
}

AudioRenderStage::~AudioRenderStage() {
    // Delete the framebuffer if it was generated
    if (m_framebuffer != 0) {
        glDeleteFramebuffers(1, &m_framebuffer);
        m_framebuffer = 0;
    }

    printf("Deleting render stage %d (%s)\n", gid, name.c_str());
}

bool AudioRenderStage::initialize() {
    // Rebuild shader sources and create shader program with all plugin imports
    rebuild_shader_sources();

    // Initialize the shader program
    if (!initialize_shader_program()) {
        return false;
    }

    // Initialize the framebuffer
    if (!initialize_framebuffer()) {
        return false;
    }

    // Compile the parameters
    if (!initialize_stage_parameters()) {
        return false;
    }

    m_initialized = true;

    return true;
}

bool AudioRenderStage::initialize_shader_program() {
    if (!m_shader_program->initialize()) {
        std::cerr << "Error: Failed to initialize shader program." << std::endl;
        return false;
    }

    // Bind any uniform blocks that have been registered for this GL context.
    AudioUniformBufferParameter::bind_registered_blocks(m_shader_program->get_program());

    return true;
}

bool AudioRenderStage::initialize_framebuffer() {
    glGenFramebuffers(1, &m_framebuffer);
    if (m_framebuffer == 0) {
        std::cerr << "Error: Failed to generate framebuffer." << std::endl;
        return false;
    }
    return true;
}

bool AudioRenderStage::initialize_stage_parameters() {
    if (m_framebuffer == 0) {
        std::cerr << "Error: Framebuffer not generated." << std::endl;
        return false;
    }
    if (m_parameters.size() == 0) {
        std::cerr << "Error: No parameters to compile." << std::endl;
        return false;
    }


    for (auto &[name, param] : m_parameters) {
        // Link parameter to the stage
        if (!param->initialize(m_framebuffer, m_shader_program.get())) {
            printf("Error: Failed to initialize parameter %s\n", param->name.c_str());
            return false;
        }
    }
    return true;
}

bool AudioRenderStage::bind() {
    // Make sure is initialized
    if (!m_initialized) {
        std::cerr << "Error: Render stage not initialized." << std::endl;
        return false;
    }

    // Bind the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    // bind the parameters to the next render stage
    for (auto & [name, param] : m_parameters) {
        if (!param->bind()) {
            printf("Error: Failed to process linked parameters for %s\n", param->name.c_str());
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

bool AudioRenderStage::unbind() {
    // Unbind the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // unbind the parameters to the next render stage
    for (auto & [name, param] : m_parameters) {
        if (!param->unbind()) {
            printf("Error: Failed to process linked parameters for %s\n", param->name.c_str());
            return false;
        }
    }
    return true;
}

void AudioRenderStage::render(const unsigned int time) {
    m_time = time;

    // Use the shader program of the stage
    glUseProgram(m_shader_program->get_program());

    // Bind the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

    // Render parameters
    for (auto & [name, param] : m_parameters) {
        param->render();
    }

    // CRITICAL: glDrawBuffers array indices map to shader output layout locations.
    // drawBuffers[0] maps to layout(location=0), drawBuffers[1] maps to layout(location=1), etc.
    // The array must have exactly as many elements as there are shader outputs, and indices must match.
    // 
    // Find the last valid (non-GL_NONE) entry to determine the actual array size needed.
    // This avoids creating a new vector on every render call while handling any GL_NONE entries.
    if (!m_draw_buffers.empty()) {
        // Find the highest index with a valid attachment
        size_t max_index = m_draw_buffers.size() - 1;
        while (max_index > 0 && m_draw_buffers[max_index] == GL_NONE) {
            --max_index;
        }
        // Only call glDrawBuffers if we have at least one valid attachment
        if (m_draw_buffers[max_index] != GL_NONE) {
            glDrawBuffers(max_index + 1, m_draw_buffers.data());
        }
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // unbind the framebuffer and texture and shader program
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);
}

bool AudioRenderStage::add_parameter(AudioParameter * parameter) {
    // Put in the parameter list
    m_parameters[parameter->name] = std::unique_ptr<AudioParameter>(parameter);

    // Add to the input or output list
    if (parameter->connection_type == AudioParameter::ConnectionType::INPUT) {
        m_input_parameters.push_back(m_parameters[parameter->name].get());
    } else if (parameter->connection_type == AudioParameter::ConnectionType::OUTPUT) {
        m_output_parameters.push_back(m_parameters[parameter->name].get());
        
        // If it's an AudioTexture2DParameter output, add to draw buffers
        if (auto * texture_param = dynamic_cast<AudioTexture2DParameter *>(parameter)) {
            GLenum draw_buffer = GL_COLOR_ATTACHMENT0 + texture_param->get_color_attachment();
            
            // CRITICAL: glDrawBuffers array indices map to shader output layout locations.
            // drawBuffers[0] maps to layout(location=0), drawBuffers[1] maps to layout(location=1), etc.
            // The array index MUST match the shader output layout location.
            // Since color_attachment is assigned sequentially (0, 1, 2, ...) and layout locations
            // are also sequential, we insert at the color_attachment index.
            size_t index = texture_param->get_color_attachment();
            if (index >= m_draw_buffers.size()) {
                // Resize to accommodate the new index (fill new slots with GL_NONE)
                m_draw_buffers.resize(index + 1, GL_NONE);
            }
            m_draw_buffers[index] = draw_buffer;
        }
    }
    return true;
}

bool AudioRenderStage::remove_parameter(const std::string & name) {
    // Check if the parameter exists
    if (m_parameters.find(name) == m_parameters.end()) {
        std::cerr << "Error: Parameter " << name << " not found." << std::endl;
        return false;
    }

    auto* param = m_parameters[name].get();

    // If it's an AudioTexture2DParameter output, remove from draw buffers
    if (param->connection_type == AudioParameter::ConnectionType::OUTPUT) {
        if (auto * texture_param = dynamic_cast<AudioTexture2DParameter *>(param)) {
            size_t index = texture_param->get_color_attachment();
            if (index < m_draw_buffers.size()) {
                m_draw_buffers[index] = GL_NONE;
                // Trim trailing GL_NONE entries to keep the array compact
                // (but keep GL_NONE in the middle if there are gaps, as they represent missing layout locations)
                while (!m_draw_buffers.empty() && m_draw_buffers.back() == GL_NONE) {
                    m_draw_buffers.pop_back();
                }
            }
        }
    }

    // Remove from input or output list
    auto it = std::find_if(m_input_parameters.begin(), m_input_parameters.end(),
                           [&name](AudioParameter * param) { return param->name == name; });
    if (it != m_input_parameters.end()) {
        m_input_parameters.erase(it);
    } else {
        it = std::find_if(m_output_parameters.begin(), m_output_parameters.end(),
                          [&name](AudioParameter * param) { return param->name == name; });
        if (it != m_output_parameters.end()) {
            m_output_parameters.erase(it);
        }
    }

    // Remove from the parameter map
    m_parameters.erase(name);

    return true;
}

AudioParameter * AudioRenderStage::find_parameter(const std::string name) const {
    if (m_parameters.find(name) != m_parameters.end()) {
        return m_parameters.at(name).get();
    }
    return nullptr;
}  

bool AudioRenderStage::register_plugin(AudioRenderStagePlugin* plugin) {
    if (!plugin) {
        std::cerr << "Error: Cannot register null plugin." << std::endl;
        return false;
    }

    if (m_initialized) {
        std::cerr << "Error: Cannot register plugin after initialization." << std::endl;
        return false;
    }

    // Store plugin
    m_plugins.push_back(plugin);

    // Create parameters for the plugin (pass texture counts by reference)
    plugin->create_parameters(m_active_texture_count, m_color_attachment_count);

    // Add all parameters from the plugin
    auto params = plugin->get_parameters();
    for (auto* param : params) {
        if (!this->add_parameter(param)) {
            std::cerr << "Error: Failed to add parameter " << param->name << " from plugin." << std::endl;
            return false;
        }
    }

    return true;
}

const std::string AudioRenderStage::get_shader_source(const std::string & file_path) {
    // Open file
    FILE * file = fopen(file_path.c_str(), "r");
    if (!file) {
        std::cerr << "Error: Failed to open file " << file_path << std::endl;
        return nullptr;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    // load the vertex shader source code
    GLchar * source = new GLchar[length + 1];
    fread(source, 1, length, file);
    source[length] = '\0';

    // Close file
    fclose(file);

    return std::string(source);
}

const std::string AudioRenderStage::combine_shader_source(const std::vector<std::string> & import_paths, const std::string & shader_path) {
    std::string combined_source = "";
    bool version_added = false;
    
    for (auto & import_path : import_paths) {
        std::string import_source = get_shader_source(import_path);
        
        // Check if this import contains a #version directive
        if (import_source.find("#version") != std::string::npos) {
            if (!version_added) {
                // Find the first #version line and add it
                size_t version_pos = import_source.find("#version");
                size_t newline_pos = import_source.find('\n', version_pos);
                if (newline_pos != std::string::npos) {
                    combined_source += import_source.substr(version_pos, newline_pos - version_pos + 1);
                    // Add the rest of the import source without the #version line
                    combined_source += import_source.substr(newline_pos + 1);
                } else {
                    combined_source += import_source;
                }
                version_added = true;
            } else {
                // Remove #version line from subsequent imports
                size_t version_pos = import_source.find("#version");
                size_t newline_pos = import_source.find('\n', version_pos);
                if (newline_pos != std::string::npos) {
                    combined_source += import_source.substr(newline_pos + 1);
                } else {
                    combined_source += import_source;
                }
            }
        } else {
            combined_source += import_source;
        }
        combined_source += "\n";
    }
    
    std::string shader_source = get_shader_source(shader_path);
    
    // Check if the main shader has a #version directive
    if (shader_source.find("#version") != std::string::npos) {
        if (!version_added) {
            // If no version was added yet, keep the one from the main shader
            combined_source += shader_source;
        } else {
            // Remove #version line from main shader
            size_t version_pos = shader_source.find("#version");
            size_t newline_pos = shader_source.find('\n', version_pos);
            if (newline_pos != std::string::npos) {
                combined_source += shader_source.substr(newline_pos + 1);
            } else {
                combined_source += shader_source;
            }
        }
    } else {
        combined_source += shader_source;
    }

    return combined_source;
}

const std::string AudioRenderStage::combine_shader_source_with_string(const std::vector<std::string> & import_paths, const std::string & shader_source) {
    std::string combined_source = "";
    bool version_added = false;
    
    for (auto & import_path : import_paths) {
        std::string import_source = get_shader_source(import_path);
        
        // Check if this import contains a #version directive
        if (import_source.find("#version") != std::string::npos) {
            if (!version_added) {
                // Find the first #version line and add it
                size_t version_pos = import_source.find("#version");
                size_t newline_pos = import_source.find('\n', version_pos);
                if (newline_pos != std::string::npos) {
                    combined_source += import_source.substr(version_pos, newline_pos - version_pos + 1);
                    // Add the rest of the import source without the #version line
                    combined_source += import_source.substr(newline_pos + 1);
                } else {
                    combined_source += import_source;
                }
                version_added = true;
            } else {
                // Remove #version line from subsequent imports
                size_t version_pos = import_source.find("#version");
                size_t newline_pos = import_source.find('\n', version_pos);
                if (newline_pos != std::string::npos) {
                    combined_source += import_source.substr(newline_pos + 1);
                } else {
                    combined_source += import_source;
                }
            }
        } else {
            combined_source += import_source;
        }
        combined_source += "\n";
    }
    
    // Use the provided shader source string instead of reading from file
    // Check if the shader source has a #version directive
    if (shader_source.find("#version") != std::string::npos) {
        if (!version_added) {
            // If no version was added yet, keep the one from the shader source
            combined_source += shader_source;
        } else {
            // Remove #version line from shader source
            size_t version_pos = shader_source.find("#version");
            size_t newline_pos = shader_source.find('\n', version_pos);
            if (newline_pos != std::string::npos) {
                combined_source += shader_source.substr(newline_pos + 1);
            } else {
                combined_source += shader_source;
            }
        }
    } else {
        combined_source += shader_source;
    }

    return combined_source;
}

const std::vector<AudioParameter *> AudioRenderStage::get_output_interface() {
    std::vector<AudioParameter *> outputs;
    
    outputs.push_back(find_parameter("output_audio_texture"));
    return outputs;
}

bool AudioRenderStage::release_output_interface(AudioRenderStage * next_stage)
{
    auto output = find_parameter("output_audio_texture");

    if (!output->is_connected()) {
        printf("Output parameter %s in render stage %d is already unlinked\n", output->name.c_str(), this->gid);
        return false;
    }

    if (!output->unlink()) {
        printf("Failed to unlink parameter %s in render stage %d and %d\n",
               output->name.c_str(),this->gid, next_stage->gid);
        return false;
    }

    output->clear_value();

    return true;
        }
    
const std::vector<AudioParameter *> AudioRenderStage::get_stream_interface()
{
    std::vector<AudioParameter *> inputs;
    
    inputs.push_back(find_parameter("stream_audio_texture"));
    return inputs;
        }
        
        bool AudioRenderStage::release_stream_interface(AudioRenderStage * prev_stage)
{
    // Clear all the stream parameters
    auto * stream = find_parameter("stream_audio_texture");

    stream->clear_value();

    return true;
}

bool AudioRenderStage::connect_render_stage(AudioRenderStage * next_stage) {
    auto output_parameters = get_output_interface();
    auto stream_parameters = next_stage->get_stream_interface();

    // Check they are the right size
    if (output_parameters.size() != stream_parameters.size()) {
        printf("Input and output sizes do not match for connecting render stage %d and %d\n", this->gid, next_stage->gid);
        return false;
    }

    for (int i=0; i<output_parameters.size(); i++){
        if (!output_parameters[i]){
            printf("Could not find output parameter %s in render stage %d\n", output_parameters[i]->name.c_str(), this->gid);
            return false;
        }

        if (output_parameters[i]->is_connected()) {
            printf("Output parameter %s in render stage %d is already linked\n", output_parameters[i]->name.c_str(), this->gid);
            return false;
        }

        // Need to make sure they are the same parameter type
        if (typeid(output_parameters[i]) != typeid(stream_parameters[i])) {
            printf("Parameter type %s and %s in render stage %d and %d do not match\n",
                   output_parameters[i]->name.c_str(), stream_parameters[i]->name.c_str(), this->gid, next_stage->gid);
            return false;
        }

        // too lazy to check the sizes for now

        if (!output_parameters[i]->link(stream_parameters[i])) {
            printf("Failed to link parameter %s and %s in render stage %d and %d\n",
                   output_parameters[i]->name.c_str(), stream_parameters[i]->name.c_str(), this->gid, next_stage->gid);
            return false;
        }

        //if (m_initialized && !bind()) {
        //    printf("Failed to bind render stage %d\n", this->gid);
        //    return false;
        //}
    }

    m_connected_output_render_stages.insert(next_stage);
    next_stage->m_connected_stream_render_stages.insert(this);

    return true;
}

bool AudioRenderStage::disconnect_render_stage(AudioRenderStage * next_stage) {
    // Check if it connected
    if (m_connected_output_render_stages.find(next_stage) == m_connected_output_render_stages.end()) {
        printf("Render stage %d is not connected to %d\n", this->gid, next_stage->gid);
        return false;
    }

    // Check that next_stage is the right one
    if (next_stage != *m_connected_output_render_stages.begin()){
        printf("Trying to disconnect render stage %d not connect to %d\n", next_stage->gid, this->gid);
        return false;
    }

    if (!next_stage->release_stream_interface(this)) {
        printf("Failed to release stream interface between render stage %d and %d\n", this->gid, next_stage->gid);
        return false;
    }
    if (!this->release_output_interface(next_stage)) {
        printf("Failed to release output interface between render stage %d and %d\n", this->gid, next_stage->gid);
        return false;
    }

    m_connected_output_render_stages.extract(next_stage);
    next_stage->m_connected_stream_render_stages.extract(this);

    return true;
}

bool AudioRenderStage::disconnect_render_stage() {
    if (m_connected_output_render_stages.size() == 1){
        return disconnect_render_stage(*m_connected_output_render_stages.begin());
    }
    printf("render stage %d is not connected\n", this->gid);
    return false;
}

void AudioRenderStage::print_input_textures() {
    for (auto & [name, input] : m_parameters) {
        if (input->connection_type == AudioParameter::ConnectionType::PASSTHROUGH) {
            if (dynamic_cast<AudioTexture2DParameter *>(input.get())) {
                printf("Input 2DTexture parameter %s in render stage %d\n", input->name.c_str(), this->gid);
                // Print the data
                for (int i = 0; i < 5; i++) {
                    printf("%f ", ((float *)input->get_value())[i]);
                }
                printf("\n");
            }
        }
    }
}

void AudioRenderStage::clear_input_textures() {
    for (auto & [name, input] : m_parameters) {
        if (input->connection_type == AudioParameter::ConnectionType::PASSTHROUGH) {
            if (dynamic_cast<AudioTexture2DParameter *>(input.get())) {
                printf("Clearing input 2DTexture parameter %s in render stage %d\n", input->name.c_str(), this->gid);
                input->clear_value();
            }
        }
    }
}

void AudioRenderStage::print_output_textures() {
    for (auto & [name, output] : m_parameters) {
        if (output->connection_type == AudioParameter::ConnectionType::OUTPUT) {
            if (auto * texture_param = dynamic_cast<AudioTexture2DParameter *>(output.get())) {
                printf("Output 2DTexture parameter %s in render stage %d\n", output->name.c_str(), this->gid);
                // Bind the framebuffer to read the output
                glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

                // Set the read buffer to the correct color attachment
                glReadBuffer(GL_COLOR_ATTACHMENT0 + texture_param->get_color_attachment());

                // Read the first few pixels from the framebuffer
                float pixel_data[5];
                glReadPixels(0, 0, 5, 1, GL_RED, GL_FLOAT, pixel_data);

                // Print the data
                for (int i = 0; i < 5; i++) {
                    printf("%f ", pixel_data[i]);
                }
                printf("\n");

                // Unbind the framebuffer
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }
        }
    }
}

void AudioRenderStage::setup_default_parameters() {
    int width = frames_per_buffer;
    int height = num_channels;

    auto stream_audio_texture =
        new AudioTexture2DParameter("stream_audio_texture",
                                    AudioParameter::ConnectionType::PASSTHROUGH,
                                    width, height,
                                    m_active_texture_count++,
                                    0,
                                    GL_NEAREST);
    auto output_audio_texture =
        new AudioTexture2DParameter("output_audio_texture",
                                    AudioParameter::ConnectionType::OUTPUT,
                                    width, height,
                                    0,
                                    m_color_attachment_count++, GL_NEAREST);
    auto debug_audio_texture =
        new AudioTexture2DParameter("debug_audio_texture",
                                    AudioParameter::ConnectionType::OUTPUT,
                                    width, height,
                                    0,
                                    m_color_attachment_count++, GL_NEAREST);

    auto buffer_size =
        new AudioIntParameter("buffer_size",
                  AudioParameter::ConnectionType::INITIALIZATION);
    buffer_size->set_value(frames_per_buffer);

    auto n_channels = 
        new AudioIntParameter("num_channels",
                  AudioParameter::ConnectionType::INITIALIZATION);
    n_channels->set_value(num_channels);

    auto samp_rate =
        new AudioIntParameter("sample_rate",
                  AudioParameter::ConnectionType::INITIALIZATION);
    samp_rate->set_value(sample_rate);
    
    if (!this->add_parameter(output_audio_texture)) {
        std::cerr << "Failed to add output_audio_texture" << std::endl;
    }
    if (!this->add_parameter(stream_audio_texture)) {
        std::cerr << "Failed to add stream_audio_texture" << std::endl;
    }
    if (!this->add_parameter(debug_audio_texture)) {
        std::cerr << "Failed to add debug_audio_texture" << std::endl;
    }
    if (!this->add_parameter(buffer_size)) {
        std::cerr << "Failed to add buffer_size" << std::endl;
    }
    if (!this->add_parameter(n_channels)) {
        std::cerr << "Failed to add num_channels" << std::endl;
    }
    if (!this->add_parameter(samp_rate)) {
        std::cerr << "Failed to add sample_rate" << std::endl;
    }
}

void AudioRenderStage::clear_output_textures() {
    for (auto & [name, output] : m_parameters) {
        if (output->connection_type == AudioParameter::ConnectionType::OUTPUT) {
            if (auto * texture_param = dynamic_cast<AudioTexture2DParameter *>(output.get())) {
                printf("Clearing output 2DTexture parameter %s in render stage %d\n", output->name.c_str(), this->gid);
                // Bind the framebuffer
                glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

                // Set the draw buffer to the correct color attachment
                GLenum draw_buffers[] = {GL_COLOR_ATTACHMENT0 + texture_param->get_color_attachment()};
                glDrawBuffers(1, draw_buffers);

                // Clear the color attachment
                glClear(GL_COLOR_BUFFER_BIT);

                // Unbind the framebuffer
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }
        }
    }
}

void AudioRenderStage::rebuild_shader_sources() {
    // Start with initial imports (no plugin-specific processing needed for these)
    std::string combined_frag_source = "";
    std::string combined_vert_source = "";
    bool frag_version_added = false;
    bool vert_version_added = false;
    
    // Process initial fragment shader imports
    for (const auto& import_path : m_initial_frag_shader_imports) {
        std::string import_source = get_shader_source(import_path);
        
        // Handle #version directive
        if (import_source.find("#version") != std::string::npos) {
            if (!frag_version_added) {
                size_t version_pos = import_source.find("#version");
                size_t newline_pos = import_source.find('\n', version_pos);
                if (newline_pos != std::string::npos) {
                    combined_frag_source += import_source.substr(version_pos, newline_pos - version_pos + 1);
                    combined_frag_source += import_source.substr(newline_pos + 1);
                } else {
                    combined_frag_source += import_source;
                }
                frag_version_added = true;
            } else {
                size_t version_pos = import_source.find("#version");
                size_t newline_pos = import_source.find('\n', version_pos);
                if (newline_pos != std::string::npos) {
                    combined_frag_source += import_source.substr(newline_pos + 1);
                } else {
                    combined_frag_source += import_source;
                }
            }
        } else {
            combined_frag_source += import_source;
        }
        combined_frag_source += "\n";
    }
    
    // Process initial vertex shader imports
    for (const auto& import_path : m_initial_vert_shader_imports) {
        std::string import_source = get_shader_source(import_path);
        
        // Handle #version directive
        if (import_source.find("#version") != std::string::npos) {
            if (!vert_version_added) {
                size_t version_pos = import_source.find("#version");
                size_t newline_pos = import_source.find('\n', version_pos);
                if (newline_pos != std::string::npos) {
                    combined_vert_source += import_source.substr(version_pos, newline_pos - version_pos + 1);
                    combined_vert_source += import_source.substr(newline_pos + 1);
                } else {
                    combined_vert_source += import_source;
                }
                vert_version_added = true;
            } else {
                size_t version_pos = import_source.find("#version");
                size_t newline_pos = import_source.find('\n', version_pos);
                if (newline_pos != std::string::npos) {
                    combined_vert_source += import_source.substr(newline_pos + 1);
                } else {
                    combined_vert_source += import_source;
                }
            }
        } else {
            combined_vert_source += import_source;
        }
        combined_vert_source += "\n";
    }

    // Process each plugin's shader imports separately - plugins handle their own replacements
    for (auto* registered_plugin : m_plugins) {
        // Process fragment shader imports for this plugin
        auto frag_imports = registered_plugin->get_fragment_shader_imports();
        for (const auto& import_path : frag_imports) {
            // Get processed shader source from plugin (with replacements already applied)
            std::string import_source = registered_plugin->get_processed_fragment_shader_source(import_path);
            
            // Handle #version directive (skip if already added)
            if (import_source.find("#version") != std::string::npos) {
                if (!frag_version_added) {
                    size_t version_pos = import_source.find("#version");
                    size_t newline_pos = import_source.find('\n', version_pos);
                    if (newline_pos != std::string::npos) {
                        combined_frag_source += import_source.substr(version_pos, newline_pos - version_pos + 1);
                        combined_frag_source += import_source.substr(newline_pos + 1);
                    } else {
                        combined_frag_source += import_source;
                    }
                    frag_version_added = true;
                } else {
                    size_t version_pos = import_source.find("#version");
                    size_t newline_pos = import_source.find('\n', version_pos);
                    if (newline_pos != std::string::npos) {
                        combined_frag_source += import_source.substr(newline_pos + 1);
                    } else {
                        combined_frag_source += import_source;
                    }
                }
            } else {
                combined_frag_source += import_source;
            }
            combined_frag_source += "\n";
        }
        
        // Process vertex shader imports for this plugin
        auto vert_imports = registered_plugin->get_vertex_shader_imports();
        for (const auto& import_path : vert_imports) {
            // Get processed shader source from plugin (with replacements already applied)
            std::string import_source = registered_plugin->get_processed_vertex_shader_source(import_path);
            
            // Handle #version directive (skip if already added)
            if (import_source.find("#version") != std::string::npos) {
                if (!vert_version_added) {
                    size_t version_pos = import_source.find("#version");
                    size_t newline_pos = import_source.find('\n', version_pos);
                    if (newline_pos != std::string::npos) {
                        combined_vert_source += import_source.substr(version_pos, newline_pos - version_pos + 1);
                        combined_vert_source += import_source.substr(newline_pos + 1);
                    } else {
                        combined_vert_source += import_source;
                    }
                    vert_version_added = true;
                } else {
                    size_t version_pos = import_source.find("#version");
                    size_t newline_pos = import_source.find('\n', version_pos);
                    if (newline_pos != std::string::npos) {
                        combined_vert_source += import_source.substr(newline_pos + 1);
                    } else {
                        combined_vert_source += import_source;
                    }
                }
            } else {
                combined_vert_source += import_source;
            }
            combined_vert_source += "\n";
        }
    }

    // Add main shader sources
    if (m_uses_shader_string) {
        std::string main_shader = m_fragment_shader_source_string;
        if (main_shader.find("#version") != std::string::npos) {
            if (!frag_version_added) {
                combined_frag_source += main_shader;
                frag_version_added = true;
            } else {
                size_t version_pos = main_shader.find("#version");
                size_t newline_pos = main_shader.find('\n', version_pos);
                if (newline_pos != std::string::npos) {
                    combined_frag_source += main_shader.substr(newline_pos + 1);
                } else {
                    combined_frag_source += main_shader;
                }
            }
        } else {
            combined_frag_source += main_shader;
        }
    } else {
        std::string main_shader = get_shader_source(m_fragment_shader_path);
        if (main_shader.find("#version") != std::string::npos) {
            if (!frag_version_added) {
                combined_frag_source += main_shader;
                frag_version_added = true;
            } else {
                size_t version_pos = main_shader.find("#version");
                size_t newline_pos = main_shader.find('\n', version_pos);
                if (newline_pos != std::string::npos) {
                    combined_frag_source += main_shader.substr(newline_pos + 1);
                } else {
                    combined_frag_source += main_shader;
                }
            }
        } else {
            combined_frag_source += main_shader;
        }
    }
    
    std::string vert_shader = get_shader_source(m_vertex_shader_path);
    if (vert_shader.find("#version") != std::string::npos) {
        if (!vert_version_added) {
            combined_vert_source += vert_shader;
            vert_version_added = true;
        } else {
            size_t version_pos = vert_shader.find("#version");
            size_t newline_pos = vert_shader.find('\n', version_pos);
            if (newline_pos != std::string::npos) {
                combined_vert_source += vert_shader.substr(newline_pos + 1);
            } else {
                combined_vert_source += vert_shader;
            }
        }
    } else {
        combined_vert_source += vert_shader;
    }

    m_fragment_shader_source = combined_frag_source;
    m_vertex_shader_source = combined_vert_source;

    // Create shader program with updated sources
    m_shader_program = std::make_unique<AudioShaderProgram>(m_vertex_shader_source, m_fragment_shader_source);
}