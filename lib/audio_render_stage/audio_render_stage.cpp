#include <iostream>
#include <cstring>
#include <string>

#include "audio_core/audio_renderer.h"
#include "audio_render_stage/audio_render_stage.h"
#include "audio_parameter/audio_texture2d_parameter.h"

AudioRenderStage::AudioRenderStage(const unsigned int frames_per_buffer,
                                   const unsigned int sample_rate,
                                   const unsigned int num_channels,
                                   const char * fragment_shader_path,
                                   const char * vertex_shader_path)
                                  : gid(generate_id()),
                                    m_frames_per_buffer(frames_per_buffer),
                                    m_sample_rate(sample_rate),
                                    m_num_channels(num_channels),
                                    m_vertex_shader_source(get_shader_source(vertex_shader_path)),
                                    m_fragment_shader_source(get_shader_source(fragment_shader_path)) {
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
    
    if (!this->add_parameter(output_audio_texture)) {
        std::cerr << "Failed to add output_audio_texture" << std::endl;
    }
    if (!this->add_parameter(stream_audio_texture)) {
        std::cerr << "Failed to add stream_audio_texture" << std::endl;
    }
}

AudioRenderStage::~AudioRenderStage() {
    // Delete the framebuffer
    glDeleteFramebuffers(1, &m_framebuffer);
    // Delete shader program
    glDeleteProgram(m_shader_program);
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
    // Create the vertex and fragment shaders
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    // Compile the vertex shader
    glShaderSource(vertex_shader, 1, &m_vertex_shader_source, NULL);
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
    glShaderSource(fragment_shader, 1, &m_fragment_shader_source, NULL);
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
            printf("Error: Failed to initialize parameter %s\n", param->name);
            return false;
        }
    }
    return true;
}

bool AudioRenderStage::bind_shader_stage() {
    for (auto & [name, param] : m_parameters) {
        if (!param->bind_parameter()) {
            printf("Error: Failed to process linked parameters for %s\n", param->name);
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
    return true;
}

AudioParameter * AudioRenderStage::find_parameter(const char * name) const {
    if (m_parameters.find(name) != m_parameters.end()) {
        return m_parameters.at(name).get();
    }
    return nullptr;
}  

const GLchar * AudioRenderStage::get_shader_source(const char * file_path) {
    // Open file
    FILE * file = fopen(file_path, "r");
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

    return source;
}