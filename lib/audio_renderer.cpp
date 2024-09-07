#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <iostream>
#include <vector>
#include <cstring>

#include "audio_renderer.h"

AudioRenderer * AudioRenderer::instance = nullptr;

bool AudioRenderer::add_render_stage(AudioRenderStage& render_stage)
{
    if (m_initialized) {
        std::cerr << "Error: Cannot add render stage after initialization." << std::endl;
        return false;
    }
    // Add the render stage to the list of render stages
    m_render_stages.push_back(std::shared_ptr<AudioRenderStage>(&render_stage, [](AudioRenderStage*){}));
    m_num_stages = m_render_stages.size();

    return true;
}

void initialize_glut(unsigned int window_width, unsigned int window_height) {
    printf("Initializing GLUT\n");

    int argc = 0;
    char** argv = nullptr;
    glutInit(&argc, argv);

    // Init the OpenGL context
    glutInitDisplayMode(GLUT_RGBA | GLUT_SINGLE);
    glutInitWindowSize(window_width, window_height);
    //glutInitWindowSize(buffer_size, 100);
    glutCreateWindow("Audio Processing");
    // Set the window close action to continue execution
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);
}

bool create_quad(GLuint &VAO, GLuint &VBO, GLuint &PBO, const unsigned int num_channels, const unsigned int buffer_size) {
    // Just a default set of vertices to cover the screen
    GLfloat vertices[] = {
        // Position    Texcoords
        -1.0f, -1.0f, 0.0f, 1.0f,  // Bottom-left
        -1.0f,  1.0f, 0.0f, 0.0f, // Top-left
         1.0f, -1.0f, 1.0f, 1.0f, // Bottom-right
         1.0f,  1.0f, 1.0f, 0.0f, // Top-right
        -1.0f,  1.0f, 0.0f, 0.0f, // Top-left
         1.0f, -1.0f, 1.0f, 1.0f // Bottom-right
    };

    // Generate Textures and Framebuffers

    // FIXME: Use shader program to do this in a different function when infra is set up
    // Generate Buffer Objects
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &PBO); // 2 PBOs for audio data

    // Bind Vertex Objects
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // Bind Pixel buffers
    glBindBuffer(GL_PIXEL_PACK_BUFFER, PBO);

    // Allocate memory for vertex data
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Allocate memory for the audio data
    glBufferData(GL_PIXEL_PACK_BUFFER, buffer_size * num_channels * sizeof(float), nullptr, GL_STREAM_READ);

    // Configure the vertex attributes
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)0); // Vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)(2 * sizeof(GLfloat))); // Texture attributes
    glEnableVertexAttribArray(1);

    // Unbind the buffers
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "Failed to initialize OpenGL: " << error << std::endl;
        return false;
    }
    return true;
}

bool AudioRenderer::init(const unsigned int buffer_size, const unsigned int sample_rate, const unsigned int num_channels) {
    if (m_initialized) {
        std::cerr << "Error: Audio renderer already initialized." << std::endl;
        return false;
    }

    this->m_buffer_size = buffer_size;
    this->m_num_channels = num_channels;
    this->m_sample_rate = sample_rate;

    // Check that at least one stage has been added
    if (m_num_stages == 0) {
        std::cerr << "Error: No render stages added." << std::endl;
        return false;
    }

    // Initialize GLUT
    initialize_glut(buffer_size*num_channels, 100);
    
    // Initialize GLEW
    GLenum glewInitResult = glewInit();
    if (glewInitResult != GLEW_OK) {        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(glewInitResult) << std::endl;
        return false;
    }

    // Print the OpenGL version
    const GLubyte* glVersion = glGetString(GL_VERSION);
    const GLubyte* glslVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);
    std::cout << "OpenGL Version: " << glVersion << std::endl;
    std::cout << "GLSL Version: " << glslVersion << std::endl;

    // Set GL settings for audio rendering
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);

    // initialize all the render stages
    for (auto& stage : m_render_stages) {
        bool return_value = stage->init();
        if (return_value == false) {
            std::cerr << "Failed to compile render stages" << std::endl;
            return false;
        }
    }
    std::cout << "Render stages compiled successfully." << std::endl;

    // Link the stages together by linking and checking the framebuffers and textures
    for (int i = 0; i < (int)m_num_stages; i++) { // Link all except last stage because it's output
        bool return_value;
        if ((unsigned)i == m_num_stages - 1) {
            printf("Tying off stage %d\n", i);
            return_value = AudioRenderStage::tie_off_output_stage(*m_render_stages[i]);
        } else{
            printf("Linking stage %d to stage %d\n", i, i+1);
            return_value = AudioRenderStage::link_stages(*m_render_stages[i], *m_render_stages[i+1]);
        }
        if (return_value == false) {
            std::cerr << "Failed to link stages." << std::endl;
            return false;
        }
    }

    // Check that the parameters in the last stage of the render stages is only output
    auto final_output_params = m_render_stages[m_num_stages - 1]->get_parameters_with_type(AudioRenderStageParameter::Type::STREAM_OUTPUT);
    if (final_output_params.size() != 1) {
        std::cerr << "Error: Final stage must have only one output parameter." << std::endl;
        return false;
    }
    // The size of the output param must be the same as the buffer size
    if (final_output_params.begin()->second.parameter_width * final_output_params.begin()->second.parameter_height != buffer_size * num_channels) {
        std::cerr << "Error: Final stage output parameter width must be the same as the buffer size." << std::endl;
        return false;
    }

    // Create the quad
    if (!create_quad(m_VAO, m_VBO, m_PBO, num_channels, buffer_size)) {
        std::cerr << "Failed to create quad." << std::endl;
        return false;
    }

    render(0); // Render the first frame
    // Links to display and timer functions for updating audio data
    // Calculate the delay in milliseconds based on the sample rate
    int delay = buffer_size * num_channels * (1000.f / (float)sample_rate);
    // Set the timer function with the calculated delay
    glutTimerFunc(delay, render_callback, 0);
    glutDisplayFunc(display_callback);

    m_initialized = true;

    return true;
}

void AudioRenderer::calculate_frame_rate()
{
    // Calculate the frame rate
    static int previous_time = glutGet(GLUT_ELAPSED_TIME);
    int current_time = glutGet(GLUT_ELAPSED_TIME);
    m_frame_count++;

    // Calculate the elapsed time
    int elapsed_time = current_time - previous_time;

    // Print the frame rate every second
    if (elapsed_time > 1000) {
        float frame_rate = m_frame_count / (elapsed_time / 1000.f);
        // Update the window title with the frame rate
        char title[100];
        sprintf(title, "Audio Processing (%.2f fps)", frame_rate);
        glutSetWindowTitle(title);

        // Reset the frame count and previous time
        m_frame_count = 0;
        previous_time = current_time;
    }
}

void AudioRenderer::render(int value)
{
    // Clear the color of the window
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    for (int i = 0; i < (int)m_num_stages; i++) {
        // Use the shader program of the stage
        glUseProgram(m_render_stages[i]->m_shader_program);

        // bind the framebuffer of the stage
        // If on the last stage, bind the screen
        glBindFramebuffer(GL_FRAMEBUFFER, m_render_stages[i]->m_framebuffer);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // FIXME: This is a test
        m_render_stages[i]->update();

        // Bind the textures of the stage
        unsigned int texture_count = 0;
        for (auto& parameter : m_render_stages[i]->get_parameters_with_type(AudioRenderStageParameter::Type::STREAM_INPUT)) {
            // Bind the texture to the shader program
            glActiveTexture(GL_TEXTURE0 + texture_count);
            glBindTexture(GL_TEXTURE_2D, parameter.second.get_texture());
            glUniform1i(glGetUniformLocation(m_render_stages[i]->m_shader_program, parameter.second.name), texture_count);

            // If it has data, update the texture
            if (parameter.second.data != nullptr) {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                                parameter.second.parameter_width,
                                parameter.second.parameter_height,
                                parameter.second.format,
                                parameter.second.datatype,
                                *parameter.second.data);
            }

            texture_count++;
        }

        // Bind the vertex array and draw
        glBindVertexArray(m_VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

    }

    // Bind the color attachment we are on
    glReadBuffer(AudioRenderStageParameter::get_latest_color_attachment_index()); // Change to the specific color attachment you want to read from

    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PBO);
    glReadPixels(0, 0, m_buffer_size * m_num_channels, 1, GL_RED, GL_FLOAT, 0);
    const float * output_buffer_data = (float *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    if (output_buffer_data) {
        push_data_to_all_output_buffers(output_buffer_data, m_buffer_size * m_num_channels);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    // Display to screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Unbind everything
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);

    // Calculate the delay in milliseconds based on the sample rate
    int delay = m_buffer_size * m_num_channels * (1000.f / (float)m_sample_rate);

    calculate_frame_rate(); // Calculate the frame rate

    // Set the timer function with the calculated delay
    glutTimerFunc(delay, render_callback, 0);
}

void AudioRenderer::main_loop()
{
    glutMainLoop();
}

bool AudioRenderer::cleanup()
{
    // Delete the vertex array and buffer
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);

    // Delete the pixel buffer object
    glDeleteBuffers(1, &m_PBO);

    return true;
}

bool AudioRenderer::terminate()
{
    // Terminate the OpenGL context
    glutLeaveMainLoop();
    //glutDestroyWindow(glutGetWindow()); // Don't need this

    return true;
}

AudioRenderer::~AudioRenderer()
{
    // Clean up the OpenGL context
    if (!cleanup()) {
        std::cerr << "Failed to clean up OpenGL context." << std::endl;
    }
    m_output_buffers.clear();
}

AudioBuffer * AudioRenderer::get_new_output_buffer()
{
    // Create a new output buffer
    std::unique_ptr<AudioBuffer> output_buffer(new AudioBuffer(20));
    m_output_buffers.push_back(std::move(output_buffer));
    
    return m_output_buffers.back().get();
}

void AudioRenderer::push_data_to_all_output_buffers(const float * data, const unsigned int size)
{
    // Push the data to all output buffers
    for (unsigned int i = 0; i < m_output_buffers.size(); i++) {
        m_output_buffers[i]->push(data, size);
    }
}
