#include <iostream>
#include <cstring>
#include <string>
#include "audio_renderer.h"
#include "audio_render_stage.h"

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
    const GLchar * vertex_source = AudioRenderer::get_instance().get_vertex_source();
    const GLchar * fragment_source = get_fragment_source();

    // Create the vertex and fragment shaders
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    // Compile the vertex shader
    glShaderSource(vertex_shader, 1, &vertex_source, NULL);
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
    glShaderSource(fragment_shader, 1, &fragment_source, NULL);
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
    for (auto &param : m_parameters) {
        if (!param->initialize_parameter()) {
            printf("Error: Failed to initialize parameter %s\n", param->name);
            return false;
        }
    }
    return true;
}

bool AudioRenderStage::bind_shader_stage() {
    for (auto &param : m_parameters) {
        if (!param->bind_parameter()) {
            printf("Error: Failed to process linked parameters for %s\n", param->name);
            return false;
        }
        // Only increment color attachment if the parameter is an output
        if (param->connection_type == AudioParameter::ConnectionType::OUTPUT) {
            m_color_attachment ++;
        }
    }
    m_color_attachment --; // Decrement to get the correct number of color attachments
    return true;
}

void AudioRenderStage::render_render_stage() {
    m_active_texture = 0;
    
    // Use the shader program of the stage
    glUseProgram(m_shader_program);

    // Bind the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render parameters
    for (auto &param : m_parameters) {
        param->render_parameter();
        m_active_texture ++;
    }

    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    // unbind the framebuffer and texture and shader program
    // FIXME: Should unbind, but need function to re-bind it again for final step
    //glBindFramebuffer(GL_FRAMEBUFFER, 0);
    //glBindTexture(GL_TEXTURE_2D, 0);
    //glUseProgram(0);
}

bool AudioRenderStage::add_parameter(std::unique_ptr<AudioParameter> parameter) {
    // Link parameter to the stage
    parameter->link_render_stage(this);

    // Put in the parameter list
    m_parameters.push_back(std::move(parameter));
    return true;
}

AudioParameter * AudioRenderStage::find_parameter(const char * name) const {
    for (auto &param : m_parameters) {
        if (std::string(param->name) == std::string(name)) {
            return param.get();
        }
    }
    return nullptr;
}  
