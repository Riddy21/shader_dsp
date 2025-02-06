#include <iostream>
#include <cstring>
#include <typeinfo>
#include <algorithm>

#include "audio_core/audio_renderer.h"
#include "audio_render_stage/audio_render_stage.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_parameter/audio_texture2d_parameter.h"
#include "audio_core/audio_shader_program.h"

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
    
    int width = m_frames_per_buffer * m_num_channels;
    int height = 1;

    auto stream_audio_texture =
        new AudioTexture2DParameter("stream_audio_texture",
                                    AudioParameter::ConnectionType::PASSTHROUGH,
                                    width, height, // Width and height
                                    ++m_active_texture_count,
                                    0);

    auto output_audio_texture =
        new AudioTexture2DParameter("output_audio_texture",
                                    AudioParameter::ConnectionType::OUTPUT,
                                    width, height,
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

    m_shader_program = std::make_unique<AudioShaderProgram>(m_vertex_shader_source, m_fragment_shader_source);
}

AudioRenderStage::~AudioRenderStage() {
    // Delete the framebuffer
    glDeleteFramebuffers(1, &m_framebuffer);

    printf("Deleting render stage %d\n", gid);
}

bool AudioRenderStage::initialize() {
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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    std::vector<GLenum> draw_buffers;

    // Render parameters
    for (auto & [name, param] : m_parameters) {
        param->render();
        // If the type is AudioTexture2DParameter, then add to the draw buffers

        if (auto * texture_param = dynamic_cast<AudioTexture2DParameter *>(param.get())) {
            if (texture_param->connection_type == AudioParameter::ConnectionType::OUTPUT) {
                draw_buffers.push_back(GL_COLOR_ATTACHMENT0 + texture_param->get_color_attachment());
            }
        }
    }
    // sort the draw buffers
    std::sort(draw_buffers.begin(), draw_buffers.end());

    glDrawBuffers(draw_buffers.size(), draw_buffers.data());

    // Check for errors 
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        printf("Error: Framebuffer is not complete in render stage %d\n", gid);
    }

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

