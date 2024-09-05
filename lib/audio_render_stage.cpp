#include <iostream>
#include "audio_renderer.h"
#include "audio_render_stage.h"
#include "audio_render_stage_parameter.h"

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

bool AudioRenderStage::compile_parameters() {
    for (auto &param : m_parameters) {
        param.framebuffer = AudioRenderStageParameter::generate_framebuffer(param);
        param.texture = AudioRenderStageParameter::generate_texture(param);

        if (param.framebuffer == 0 || param.texture == 0) {
            return false;
        }
    }
    return true;
}

bool AudioRenderStage::link_stages(AudioRenderStage &stage1, AudioRenderStage &stage2) {
    // Check for the stage1's output parameters match the stage2's input parameters
    auto output_params = stage1.get_parameters_with_type(AudioRenderStageParameter::Type::STREAM_OUTPUT);
    auto input_params = stage2.get_parameters_with_type(AudioRenderStageParameter::Type::STREAM_INPUT);

    // link the params
    for (auto &output_param : output_params) {
        // Find in input_params
        auto input_param = input_params.find(output_param.second.link_name);
        if (input_param == input_params.end()) {
            std::cerr << "Error: Output parameter not found in input parameters." << std::endl;
            return false;
        }
        // Check if the parameters are compatible
        if (output_param.second.parameter_width != input_param->second.parameter_width ||
            output_param.second.parameter_height != input_param->second.parameter_height) {
            std::cerr << "Error: Output parameter and input parameter dimensions do not match." << std::endl;
            return false;
        }
        if (output_param.second.internal_format != input_param->second.internal_format) {
            std::cerr << "Error: Output parameter and input parameter internal formats do not match." << std::endl;
            return false;
        }
        if (output_param.second.format != input_param->second.format) {
            std::cerr << "Error: Output parameter and input parameter formats do not match." << std::endl;
            return false;
        }
        if (output_param.second.datatype != input_param->second.datatype) {
            std::cerr << "Error: Output parameter and input parameter data types do not match." << std::endl;
            return false;
        }
        if (output_param.second.data != nullptr) {
            std::cerr << "Error: Output parameter has data bound" << std::endl;
            return false;
        }
        if (input_param->second.data != nullptr) {
            std::cerr << "Error: Input parameter has data bound" << std::endl;
            return false;
        }
        if (input_param->second.is_bound || output_param.second.is_bound) {
            std::cerr << "Error: Input parameter is already bound" << std::endl;
            return false;
        }
        if (input_param->second.texture == 0) {
            std::cerr << "Error: Input parameter texture is not generated" << std::endl;
            return false;
        }
        if (output_param.second.framebuffer == 0) {
            std::cerr << "Error: Output parameter framebuffer is not generated" << std::endl;
            return false;
        }

        printf("Binding %s to %s\n", output_param.first, input_param->first);
            
        AudioRenderStageParameter::bind_framebuffer_to_texture(output_param.second, input_param->second);

        // Check the is_bound flag
        if (!output_param.second.is_bound) {
            std::cerr << "Error: Output parameter " << output_param.first << " is not bound." << std::endl;
            return false;
        }
    }

    for (auto &input_param : input_params) {
        if (!input_param.second.is_bound) {
            std::cerr << "Error: Input parameter " << input_param.first << " is not bound." << std::endl;
            return false;
        }
    }

    return true;
}

bool AudioRenderStage::add_parameter(AudioRenderStageParameter &parameter) {
    // Double check parameter values
    if (parameter.name == nullptr) {
        std::cerr << "Error: Parameter name is null." << std::endl;
        return false;
    }
    if (parameter.type == AudioRenderStageParameter::Type::STREAM_INPUT &&
        parameter.link_name == nullptr &&
        parameter.data == nullptr) {
        std::cerr << "Error: Cannot have input parameter with no connected output and no data" << std::endl;
        return false;
    }
    if (parameter.type == AudioRenderStageParameter::Type::STREAM_OUTPUT &&
        parameter.data != nullptr) {
        std::cerr << "Error: Cannot have output parameter with data" << std::endl;
        return false;
    }
    if (parameter.parameter_width == 0 || parameter.parameter_height == 0) {
        std::cerr << "Error: Parameter width or height is 0." << std::endl;
        return false;
    }
    if (parameter.format == AudioRenderStageParameter::Format::RED &&
        parameter.internal_format != AudioRenderStageParameter::InternalFormat::R8 &&
        parameter.internal_format != AudioRenderStageParameter::InternalFormat::R16F &&
        parameter.internal_format != AudioRenderStageParameter::InternalFormat::R32F) {
        std::cerr << "Error: Invalid internal format for RED format." << std::endl;
        return false;
    }

    m_parameters.push_back(parameter);
    return true;
}

std::unordered_map<const char *, AudioRenderStageParameter &> AudioRenderStage::get_parameters_with_type(AudioRenderStageParameter::Type type) {
    std::unordered_map<const char *, AudioRenderStageParameter &> output_parameters;
    for (auto &param : m_parameters) {
        if (param.type == type) {
            output_parameters.emplace(param.name, param);
        }
    }
    return output_parameters;
}
