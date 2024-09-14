#include <iostream>
#include <cstring>
#include "audio_renderer.h"
#include "audio_render_stage.h"

AudioRenderStage::~AudioRenderStage() {
    // Delete the framebuffer
    glDeleteFramebuffers(1, &m_framebuffer);
    // Delete shader program
    glDeleteProgram(m_shader_program);
}

bool AudioRenderStage::init_params() {
    // Initialize the framebuffer
    if (!initialize_framebuffer()) {
        return false;
    }

    // Compile the parameters
    if (!compile_parameters()) {
        return false;
    }

    return true;
}

bool AudioRenderStage::compile_shader_program() {
    const GLchar * vertex_source = AudioRenderer::get_instance().m_vertex_source;
    const GLchar * fragment_source = m_fragment_source;

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
    printf("Compiled vertex shader\n");
    printf("Vertex source: %s\n", vertex_source);

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
    printf("Compiled fragment shader\n");
    printf("Fragment source: %s\n", fragment_source);

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
    printf("Linked shader program\n");

    // Delete the shaders
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    m_shader_program = shader_program;
    printf("Shader program: %d\n", m_shader_program);

    return true;
}

bool AudioRenderStage::initialize_framebuffer() {
    glGenFramebuffers(1, &m_framebuffer);
    printf("Initialized framebuffer %d\n", m_framebuffer);
    if (m_framebuffer == 0) {
        std::cerr << "Error: Failed to generate framebuffer." << std::endl;
        return false;
    }
    return true;
}

bool AudioRenderStage::compile_parameters() {
    if (m_framebuffer == 0) {
        std::cerr << "Error: Framebuffer not generated." << std::endl;
        return false;
    }
    if (m_parameters.size() == 0) {
        std::cerr << "Error: No parameters to compile." << std::endl;
        return false;
    }
    for (auto &param : m_parameters) {
        if (!param->init()) {
            printf("Error: Failed to initialize parameter %s\n", param->name);
            return false;
        }
    }
    return true;
}

bool AudioRenderStage::link_params() {
    for (auto &param : m_parameters) {
        if (!param->process_linked_params()) {
            printf("Error: Failed to process linked parameters for %s\n", param->name);
            return false;
        }
        // Only increment if the parameter is an output
        if (param->connection_type == AudioParameter::ConnectionType::OUTPUT) {
            m_color_attachment++;
        }
    }
    m_color_attachment--; // Decrement the color attachment to the last one
    return true;
}

void AudioRenderStage::render_stage() {
    m_active_texture = 0;
    for (auto &param : m_parameters) {
        param->render_parameter();
        m_active_texture++;
    }

}

bool AudioRenderStage::update_fragment_source(GLchar const *fragment_source) {
    // Check if fragment source contains all nessesary variables
    // Check input parameters
    for (auto &param : m_parameters) {
        if (param->connection_type == AudioParameter::ConnectionType::INITIALIZATION) {
            if (strstr(fragment_source, ("uniform sampler2D " + std::string(param->name)).c_str()) == nullptr) {
                std::cerr << "Error: Fragment source does not contain initialization parameter " << param->name << std::endl;
                return false;
            }
        }
        if (param->connection_type == AudioParameter::ConnectionType::INPUT) {
            if (strstr(fragment_source, ("uniform sampler2D " + std::string(param->name)).c_str()) == nullptr) {
                std::cerr << "Error: Fragment source does not contain input parameter " << param->name << std::endl;
                return false;
            }
        }
        if (param->connection_type == AudioParameter::ConnectionType::OUTPUT) {
            if (strstr(fragment_source, param->name) == nullptr) {
                std::cerr << "Error: Fragment source does not contain output parameter " << param->name << std::endl;
                return false;
            }
        }
    }

    m_fragment_source = fragment_source;
    return true;
}

bool AudioRenderStage::update_audio_buffer(const float *audio_buffer, const unsigned int buffer_size) {
    if (buffer_size != m_frames_per_buffer * m_num_channels) {
        printf("Error: Buffer size must be the same as the frames per buffer * num channels\n");
        return false;
    }
    m_audio_buffer = audio_buffer;
    return true;
}

bool AudioRenderStage::add_parameter(std::unique_ptr<AudioParameter> parameter) {
    // Link parameter to the stage
    parameter->link_render_stage(this);

    // Put in the parameter list
    m_parameters.push_back(std::move(parameter));
    return true;
}
