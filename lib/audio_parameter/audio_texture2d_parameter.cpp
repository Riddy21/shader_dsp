#include <cstring>
#include <regex>
#include <string>

#include "audio_core/audio_render_stage.h"
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
        m_texture(0),
        m_PBO(0),
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

AudioTexture2DParameter::~AudioTexture2DParameter() {
    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
}

bool AudioTexture2DParameter::initialize(GLuint frame_buffer, AudioShaderProgram * shader_program) {
    m_framebuffer_linked = frame_buffer;
    m_shader_program_linked = shader_program;

    // Only require framebuffer for OUTPUT and PASSTHROUGH parameters
    if ((connection_type == ConnectionType::OUTPUT || connection_type == ConnectionType::PASSTHROUGH) && m_framebuffer_linked == 0) {
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
        // Allocate memory for the texture and initialize it with 0s
        std::vector<unsigned char> zero_data(m_parameter_width * m_parameter_height * 4, 0); // Assuming 4 channels (RGBA)
        glTexImage2D(GL_TEXTURE_2D, 0, m_internal_format, m_parameter_width, m_parameter_height, 0, m_format, m_datatype, zero_data.data());
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

    // Unbind the texture
    glBindTexture(GL_TEXTURE_2D, 0);
    // Using pixel buffer object may be faster, the extra code for that is commented out

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
    AudioTexture2DParameter* linked_param = nullptr;
    if (m_linked_parameter == nullptr) {
        // If not linked, then tie off
        linked_param = this;
    } else {
        //Check if the linked parameter is an AudioTexture2DParameter
        linked_param = dynamic_cast<AudioTexture2DParameter*>(m_linked_parameter);
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

    // Only attach to framebuffer if this is an OUTPUT parameter, or if it's a PASSTHROUGH that's linked
    // PASSTHROUGH parameters that aren't linked shouldn't attach to framebuffer (they're just input textures)
    if (connection_type == ConnectionType::OUTPUT || 
        (connection_type == ConnectionType::PASSTHROUGH && m_linked_parameter != nullptr)) {
        // Link the texture to the framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + m_color_attachment,
                               GL_TEXTURE_2D, linked_param->get_texture(), 0);
    }

    // Unbind texture
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

bool AudioTexture2DParameter::unbind() {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + m_color_attachment,
                           GL_TEXTURE_2D, 0, 0);

    return true;
}

const void * const AudioTexture2DParameter::get_value() const {
    if (connection_type == ConnectionType::INPUT) {
        return AudioParameter::get_value();
    }
    else if (connection_type == ConnectionType::PASSTHROUGH) {
        AudioTexture2DParameter* previous_param = dynamic_cast<AudioTexture2DParameter*>(m_previous_parameter);
        if (previous_param == nullptr) {
            static std::vector<float> zero_data(m_parameter_width * m_parameter_height * 4, 0.0f);
            return zero_data.data();

        }
        return previous_param->get_value();
    }
    else {
        // Bind framebuffer to read from texture
        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer_linked);
        glReadBuffer(GL_COLOR_ATTACHMENT0 + m_color_attachment);
        
        // Read pixels from framebuffer instead of using glGetTexImage
        glReadPixels(0, 0, m_parameter_width, m_parameter_height, m_format, m_datatype, m_data->get_data());
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        return m_data->get_data();
    }
}

void AudioTexture2DParameter::clear_value() {
    AudioParameter::clear_value();

    // Only clear if texture is initialized
    if (m_texture != 0) {
        glBindTexture(GL_TEXTURE_2D, m_texture);
        std::vector<unsigned char> zero_data(m_parameter_width * m_parameter_height * 4, 0); // Assuming 4 channels (RGBA)
        glTexImage2D(GL_TEXTURE_2D, 0, m_internal_format, m_parameter_width, m_parameter_height, 0, m_format, m_datatype, zero_data.data());
        glBindTexture(GL_TEXTURE_2D, 0);

        // Only clear framebuffer if it's linked
        if (m_framebuffer_linked != 0) {
            // Clear the color attachment of the frame buffer
            glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer_linked);
            // Set the draw buffer to the correct color attachment
            GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0 + m_color_attachment };
            glDrawBuffers(1, draw_buffers);

            // Clear the color attachment
            glClear(GL_COLOR_BUFFER_BIT);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }
}

std::unique_ptr<ParamData> AudioTexture2DParameter::create_param_data() {
    size_t element_count = m_parameter_width * m_parameter_height;
    size_t scale_factor = 1;

    // Adjust scale factor based on format
    switch (m_format) {
        case GL_RED:
            scale_factor = 1; // Single channel
            break;
        case GL_RG:
            scale_factor = 2; // Two channels
            break;
        case GL_RGB:
            scale_factor = 3; // Three channels
            break;
        case GL_RGBA:
            scale_factor = 4; // Four channels
            break;
        default:
            throw std::invalid_argument("Unsupported format");
    }

    return std::unique_ptr<ParamData>(new ParamFloatArrayData(element_count * scale_factor));
}