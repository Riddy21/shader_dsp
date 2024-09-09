#pragma once
#ifndef AUDIO_RENDER_STAGE_PARAMETER
#define AUDIO_RENDER_STAGE_PARAMTER

#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut.h>

/**
 * @class AudioRenderStageParameter
 * @brief Represents a parameter used in the AudioRenderStage class.
 *
 * The AudioRenderStageParameter class encapsulates the properties and functionality of a parameter used in the AudioRenderStage class.
 * It provides methods to retrieve information about the parameter, such as its name, type, dimensions, and data.
 * It also provides methods to bind the parameter to a texture or framebuffer for rendering.
 */
class AudioRenderStageParameter {
public:
    friend class AudioRenderStage; // Allow AudioRenderStage to access private members

    enum Type {
        INITIALIZATION, // pushes data to the shader at the beginning of the program
        STREAM_INPUT, // Pushes data to the shader during rendering
        STREAM_OUTPUT, // Output data from the shader
        STREAM_CONTROL
    };

    /**
     * @brief Constructor for the AudioRenderStageParameter class.
     * 
     * This constructor initializes the AudioRenderStageParameter object with the specified properties. 
     * 
     * @param name The name of the parameter.
     * @param type The type of the parameter.
     * @param parameter_width The width of the parameter.
     * @param parameter_height The height of the parameter.
     * @param data The data of the parameter.
     * @param link_name The name of the parameter to link to.
     * @param datatype The data type of the parameter.
     * @param format The format of the parameter.
     * @param internal_format The internal format of the parameter.
     */
    AudioRenderStageParameter(const char * name,
                              Type type,
                              unsigned int parameter_width,
                              unsigned int parameter_height,
                              const float ** data = nullptr,
                              const char * link_name = nullptr,
                              GLuint datatype = GL_FLOAT,
                              GLuint format = GL_RED,
                              GLuint internal_format = GL_R32F)
        : name(name),
          link_name(link_name),
          type(type),
          datatype(datatype),
          format(format),
          internal_format(internal_format),
          parameter_width(parameter_width),
          parameter_height(parameter_height),
          data(data)
    {};

    ~AudioRenderStageParameter() {
        if (m_texture != 0) {
            glDeleteTextures(1, &m_texture);
        }
    }
    
    const char * name = nullptr;
    const char * link_name = nullptr;
    const Type type = Type::INITIALIZATION;
    const GLuint datatype = GL_FLOAT;
    const GLuint format = GL_RED;
    const GLuint internal_format = GL_R32F;
    const unsigned int parameter_width = 0;
    const unsigned int parameter_height = 0;
    const float ** data = nullptr;

    GLuint get_texture() const {
        return m_texture;
    }

    GLuint get_framebuffer() const {
        return m_framebuffer;
    }

    bool is_bound() const {
        return m_is_bound;
    }

    static GLuint get_latest_color_attachment_index() {
        return m_color_attachment_index;
    }

    static void bind_framebuffer_to_texture(AudioRenderStageParameter & output_parameter,
                                            AudioRenderStageParameter & input_parameter);
    static void bind_framebuffer_to_output(AudioRenderStageParameter & output_parameter);

private:
    void generate_texture();

    static GLuint m_color_attachment_index;

    GLuint m_texture = 0;
    GLuint m_framebuffer  = 0;
    bool m_is_bound = false;
};

#endif