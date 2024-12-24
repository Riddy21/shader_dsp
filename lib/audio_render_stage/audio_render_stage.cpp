#include <iostream>
#include <cstring>
#include <typeinfo>

#include "audio_core/audio_renderer.h"
#include "audio_render_stage/audio_render_stage.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_parameter/audio_texture2d_parameter.h"

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
                                    m_frames_per_buffer(frames_per_buffer),
                                    m_sample_rate(sample_rate),
                                    m_num_channels(num_channels),
                                    m_vertex_shader_source(combine_shader_source(vert_shader_imports, vertex_shader_path)),
                                    m_fragment_shader_source(combine_shader_source(frag_shader_imports, fragment_shader_path)) {
    auto stream_audio_texture =
        new AudioTexture2DParameter("stream_audio_texture",
                                    AudioParameter::ConnectionType::PASSTHROUGH,
                                    m_frames_per_buffer * m_num_channels, 1, // Width and height
                                    ++m_active_texture_count,
                                    0);

    auto output_audio_texture =
        new AudioTexture2DParameter("output_audio_texture",
                                    AudioParameter::ConnectionType::OUTPUT,
                                    m_frames_per_buffer * m_num_channels, 1,
                                    0,
                                    ++m_color_attachment_count);

    auto buffer_size =
        new AudioIntParameter("buffer_size",
                  AudioParameter::ConnectionType::INITIALIZATION);
    buffer_size->set_value(m_frames_per_buffer*m_num_channels);

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
    if (!this->add_parameter(buffer_size)) {
        std::cerr << "Failed to add buffer_size" << std::endl;
    }
    if (!this->add_parameter(samp_rate)) {
        std::cerr << "Failed to add sample_rate" << std::endl;
    }
}

AudioRenderStage::~AudioRenderStage() {
    // Delete the framebuffer
    glDeleteFramebuffers(1, &m_framebuffer);
    // Delete shader program
    glDeleteProgram(m_shader_program);

    printf("Deleting render stage %d\n", gid);
}

bool AudioRenderStage::initialize_shader_stage() {
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

    return true;
}

bool AudioRenderStage::initialize_shader_program() {
    const GLchar * vertex_shader_source = m_vertex_shader_source.c_str();
    const GLchar * fragment_shader_source = m_fragment_shader_source.c_str();

    // Create the vertex and fragment shaders
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    // Compile the vertex shader
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);

    // Check for vertex shader compilation errors
    GLint vertex_success;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &vertex_success);
    if (!vertex_success) {
        GLchar info_log[512];
        glGetShaderInfoLog(vertex_shader, 512, NULL, info_log);
        std::cerr << "Error compiling vertex shader: " << info_log << std::endl;
        return 0;
    }
    // Compile the fragment shader
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);

    // Check for fragment shader compilation errors
    GLint fragment_success;
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &fragment_success);
    if (!fragment_success) {
        GLchar info_log[512];
        glGetShaderInfoLog(fragment_shader, 512, NULL, info_log);
        std::cerr << "Error compiling fragment shader: " << info_log << std::endl;
        return 0;
    }
    // Create the shader program
    GLuint shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);

    // Check for shader program linking errors
    GLint program_success;
    glGetProgramiv(shader_program, GL_LINK_STATUS, &program_success);
    if (!program_success) {
        GLchar info_log[512];
        glGetProgramInfoLog(shader_program, 512, NULL, info_log);
        std::cerr << "Error linking shader program: " << info_log << std::endl;
        return 0;
    }

    // Delete the shaders
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    m_shader_program = shader_program;

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
        if (!param->initialize_parameter()) {
            printf("Error: Failed to initialize parameter %s\n", param->name.c_str());
            return false;
        }
    }
    return true;
}

bool AudioRenderStage::bind_shader_stage() {
    for (auto & [name, param] : m_parameters) {
        if (!param->bind_parameter()) {
            printf("Error: Failed to process linked parameters for %s\n", param->name.c_str());
            return false;
        }
    }
    return true;
}

void AudioRenderStage::render_render_stage() {
    // Use the shader program of the stage
    glUseProgram(m_shader_program);

    // Bind the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render parameters
    for (auto & [name, param] : m_parameters) {
        param->render_parameter();
    }

    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    // unbind the framebuffer and texture and shader program
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);
}

bool AudioRenderStage::add_parameter(AudioParameter * parameter) {
    // Link parameter to the stage
    parameter->link_render_stage(this);

    // Put in the parameter list
    m_parameters[parameter->name] = std::unique_ptr<AudioParameter>(parameter);

    // Add to the input or output list
    if (parameter->connection_type == AudioParameter::ConnectionType::INPUT) {
        m_input_parameters.push_back(m_parameters[parameter->name].get());
    } else if (parameter->connection_type == AudioParameter::ConnectionType::OUTPUT) {
        m_output_parameters.push_back(m_parameters[parameter->name].get());
    }
    return true;
}
AudioParameter * AudioRenderStage::find_parameter(const std::string name) const {
    if (m_parameters.find(name) != m_parameters.end()) {
        return m_parameters.at(name).get();
    }
    return nullptr;
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
    for (auto & import_path : import_paths) {
        combined_source += get_shader_source(import_path) + "\n";
    }
    combined_source += get_shader_source(shader_path);

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

    return true;
}

const std::vector<AudioParameter *> AudioRenderStage::get_stream_interface()
{
    std::vector<AudioParameter *> inputs;
    
    inputs.push_back(find_parameter("stream_audio_texture"));
    return inputs;
}

bool AudioRenderStage::connect_render_stage(AudioRenderStage * next_stage) {
    // TODO: Consider changing these from "get output interface" to "connect_ouptut_interface"
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
    }

    m_connected_output_render_stages.insert(next_stage);
    next_stage->m_connected_stream_render_stages.insert(this);

    return true;
}

bool AudioRenderStage::disconnect_render_stage(AudioRenderStage * next_stage) {
    // Check if it's already disconnected
    if (m_connected_output_render_stages.size() == 0) {
        printf("Render stage %d is already disconnected\n", this->gid);
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

    m_connected_output_render_stages.erase(next_stage);
    next_stage->m_connected_stream_render_stages.erase(this);
    return true;
}

bool AudioRenderStage::disconnect_render_stage() {
    if (m_connected_output_render_stages.size() == 1){
        return disconnect_render_stage(*m_connected_output_render_stages.begin());
    }
    printf("render stage %d is not connected\n", this->gid);
    return false;
}


