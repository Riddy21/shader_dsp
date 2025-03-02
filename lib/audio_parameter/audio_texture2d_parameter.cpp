#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <cstring>
#include <regex>
#include <string>

#include "audio_render_stage/audio_render_stage.h"
#include "audio_parameter/audio_texture2d_parameter.h"

const float AudioTexture2DParameter::FLAT_COLOR[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

AudioTexture2DParameter::AudioTexture2DParameter(const std::string name,
                          AudioParameter::ConnectionType connection_type,
                          GLuint parameter_width,
                          GLuint parameter_height,
                          GLuint active_texture,
                          GLuint color_attachment,
                          GLuint texture_filter_type,
                          GLuint datatype,
                          GLuint format,
                          GLuint internal_format
                          )
        : AudioParameter(name, connection_type),
        m_parameter_width(parameter_width),
        m_parameter_height(parameter_height),
        m_active_texture(active_texture),
        m_color_attachment(color_attachment),
        m_filter_type(texture_filter_type),
        m_datatype(datatype),
        m_format(format),
        m_internal_format(internal_format)
{
    m_data = create_param_data();
};

bool AudioTexture2DParameter::initialize(GLuint frame_buffer, AudioShaderProgram * shader_program) {
    m_framebuffer_linked = frame_buffer;
    m_shader_program_linked = shader_program;

    if (m_framebuffer_linked == 0) {
        printf("Error: render stage linked is nullptr in parameter %s\n, cannot be a global parameter.", name.c_str());
        return false;
    }
    if (m_shader_program_linked == nullptr) {
        printf("Error: render stage linked is nullptr in parameter %s\n, cannot be a global parameter.", name.c_str());
        return false;
    }

    // Generate the texture
    glGenTextures(1, &m_texture);

    // Bind the texture
    glBindTexture(GL_TEXTURE_2D, m_texture);

    // Configure the texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_filter_type);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_filter_type);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    //glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, FLAT_COLOR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    // allocate memory for the texture
    if (connection_type == ConnectionType::INPUT || connection_type == ConnectionType::INITIALIZATION) {
        // Allocate memory for the texture and data 
        if (m_data == nullptr) {
            printf("Warning: value is nullptr when declared as input or initialization in parameter %s\n", name.c_str());
        }
        glTexImage2D(GL_TEXTURE_2D, 0, m_internal_format, m_parameter_width, m_parameter_height, 0, m_format, m_datatype, m_data->get_data());

    } else if (connection_type == ConnectionType::PASSTHROUGH || connection_type == ConnectionType::OUTPUT) {
        // Allocate memory for the texture and data but don't update the data
        glTexImage2D(GL_TEXTURE_2D, 0, m_internal_format, m_parameter_width, m_parameter_height, 0, m_format, m_datatype, nullptr);
    }
    
    GLenum status = glGetError();
    if (status != GL_NO_ERROR) {
        printf("Error: OpenGL error in parameter %s\n", name.c_str());
        return false;
    }

    // Look for the texture in the shader program
    if (connection_type == ConnectionType::OUTPUT) {
        // do regex search for output texture
        auto regex = std::regex("out .* " + std::string(name) + ";");
        if (!std::regex_search(m_shader_program_linked->get_fragment_shader_source(), regex)) {
            printf("%s ", m_shader_program_linked->get_fragment_shader_source().c_str());
            printf("Error: Could not find texture in shader program in parameter %s\n", name.c_str());
            return false;
        }
    } else {
        auto location = glGetUniformLocation(m_shader_program_linked->get_program(), name.c_str());
        if (location == -1) {
            printf("Source: %s\n", m_shader_program_linked->get_fragment_shader_source().c_str());
            printf("Error: Could not find texture in shader program in parameter %s\n", name.c_str());
            return false;
        }
    }

    // Using pixel buffer object may be faster, the extra code for that is commented out
    // Unbind the texture
    //glBindTexture(GL_TEXTURE_2D, 0);

    //// Initialize the PBO
    //glGenBuffers(1, &m_PBO);

    //// Bind the PBO
    //glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PBO);

    //// Allocate memory for the PBO
    //glBufferData(GL_PIXEL_PACK_BUFFER, (m_parameter_width * m_parameter_height) * sizeof(float), nullptr, GL_STREAM_READ);

    //// Unbind the PBO
    //glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    //// Check for errors
    //status = glGetError();
    //if (status != GL_NO_ERROR) {
    //    printf("Error: OpenGL error in initializing parameter %s\n", name);
    //    return false;
    //}

    return true;
}

void AudioTexture2DParameter::render() {
    if (connection_type == ConnectionType::OUTPUT) {
        return; // Do not need to render if output
    }
    glActiveTexture(GL_TEXTURE0 + m_active_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glUniform1i(glGetUniformLocation(m_shader_program_linked->get_program(), name.c_str()), m_active_texture);

    if (connection_type == ConnectionType::INPUT && m_update_param) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_parameter_width, m_parameter_height, m_format, m_datatype, m_data->get_data());
        m_update_param = false;
    }
}

bool AudioTexture2DParameter::bind() {
    const AudioTexture2DParameter* linked_param = nullptr;
    if (m_linked_parameter == nullptr) {
        // If not linked, then tie off
        linked_param = this;
    } else {
        //Check if the linked parameter is an AudioTexture2DParameter
        linked_param = dynamic_cast<const AudioTexture2DParameter*>(m_linked_parameter);
    }

    if (linked_param == nullptr) {
        printf("Error: Linked parameter is not an AudioTexture2DParameter in parameter %s\n", name.c_str());
        return false;
    }

    // Check if linked parameter is initialized
    if (linked_param->get_texture() == 0) {
        printf("Error: Linked parameter texture is not initialized in parameter %s\n", name.c_str());
        return false;
    }

    if (linked_param->connection_type != ConnectionType::PASSTHROUGH && linked_param != this) {
        printf("Error: Linked parameter is not a passthrough in parameter %s\n", name.c_str());
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, linked_param->get_texture());

    // Link the texture to the framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + m_color_attachment,
                           GL_TEXTURE_2D, linked_param->get_texture(), 0);

    // Unbind texture
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;

}

const void * const AudioTexture2DParameter::get_value() const {
    if (connection_type == ConnectionType::INPUT) {
        return AudioParameter::get_value();
    }
    else {
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glGetTexImage(GL_TEXTURE_2D, 0, m_format, m_datatype, m_data->get_data());
        glBindTexture(GL_TEXTURE_2D, 0);

        //  Using pixel buffer object may be faster
        //glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PBO);
        //glBindTexture(GL_TEXTURE_2D, m_texture);
        //glGetTexImage(GL_TEXTURE_2D, 0, m_format, m_datatype, nullptr);
        //glBindTexture(GL_TEXTURE_2D, 0);
        //memcpy(m_data->get_data(), glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY), m_parameter_width * m_parameter_height * sizeof(float));

        //glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        //glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

        return m_data->get_data();
    }
}