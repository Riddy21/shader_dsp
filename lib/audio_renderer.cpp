#include <GL/glew.h>
#include <GL/glut.h>
#include <iostream>
#include <vector>
#include <cstring>

#include "audio_renderer.h"

AudioRenderer * AudioRenderer::instance = nullptr;

bool AudioRenderer::add_render_stage(AudioRenderStage& render_stage)
{
    // Add the render stage to the list of render stages
    render_stages.push_back(render_stage);
    num_stages = render_stages.size();

    return true;
}

GLuint AudioRenderer::compile_shaders(const GLchar* vertex_source, const GLchar* fragment_source)
{
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

    return shader_program;
}

bool AudioRenderer::init(const unsigned int buffer_size, const unsigned int num_channels) {
    this->buffer_size = buffer_size;
    this->num_channels = num_channels;

    // Check that at least one stage has been added
    if (num_stages == 0) {
        std::cerr << "Error: No render stages added." << std::endl;
        return false;
    }

    int argc = 0;
    char** argv = nullptr;
    glutInit(&argc, argv);

    // Init the OpenGL context
    glutInitDisplayMode(GLUT_RGBA | GLUT_SINGLE);
    glutInitWindowSize(buffer_size, num_channels);
    glutCreateWindow("Audio Processing");

    // Initialize GLEW
    GLenum glewInitResult = glewInit();
    if (glewInitResult != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(glewInitResult) << std::endl;
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

    // Compile the render stage as a shader program
    for (int i = 0; i < num_stages; i++) {
        GLuint return_value = compile_shaders(vertex_source, render_stages[i].fragment_source);

        if (return_value == 0) {
            return false;
        }

        render_stages[i].shader_program = return_value;
    }

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

    // Generate Buffer Objects
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenFramebuffers(3, FBO);
    glGenTextures(3, audio_texture); // 3 textures for audio data // 0-1 for input and 2 for output
    glGenBuffers(1, &PBO); // 2 PBOs for audio data

    // flat color for the texture border
    const float flatColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };

    // Bind the Frame buffers
    for (int i = 0; i < 3; i++) {
        // Bind the right textures and framebuffers
        glBindFramebuffer(GL_FRAMEBUFFER, FBO[i]);
        glBindTexture(GL_TEXTURE_2D, audio_texture[i]);
        // Configure the texture
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, flatColor);
        // Attach the texture to the framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, audio_texture[i], 0);
        // Allocate memory for the texture
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, buffer_size, num_channels, 0, GL_RED, GL_FLOAT, nullptr);

        // Unbind everything
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

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
    // FIXME: Use shader program to do this in a different function when infra is set up
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)0); // Vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)(2 * sizeof(GLfloat))); // Texture attributes
    glEnableVertexAttribArray(1);

    // Unbind the buffers
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "Failed to initialize OpenGL: " << error << std::endl;
        return false;
    }

    // Init the output buffer
    output_buffer_data = std::vector<float>(buffer_size * num_channels);

    render(0); // Render the first frame
    // TODO: Write links to display and timer functions for updating audio data
    glutTimerFunc(0, render_callback, 0);
    glutDisplayFunc(display_callback);

    return true;
}

void AudioRenderer::render(int value)
{
    // Clear the color of the window
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);


    for (int i = 0; i < num_stages; i++) { // Do all except last stage
        // use the shader program 0 first 
        glUseProgram(render_stages[i].shader_program);

        // Bind the vertex array
        glBindVertexArray(VAO);

        // Fill the third buffer with the stream audio texture
        glActiveTexture(GL_TEXTURE1);
        glBindFramebuffer(GL_FRAMEBUFFER, FBO[2]);
        glBindTexture(GL_TEXTURE_2D, audio_texture[2]);
        glUniform1i(glGetUniformLocation(render_stages[i].shader_program, "texture2"), 1);
        // TODO: Fill with other data like time, or recorded data
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, buffer_size, num_channels, GL_RED, GL_FLOAT, &render_stages[i].audio_buffer.data()[0]);

        // Fill the second buffer with the stream audio texture
        glActiveTexture(GL_TEXTURE0);
        glBindFramebuffer(GL_FRAMEBUFFER, FBO[i % 2]);
        glBindTexture(GL_TEXTURE_2D, audio_texture[abs(i-1) % 2]);

        // Only one first stage
        if (i == 0) {
            glUniform1i(glGetUniformLocation(render_stages[i].shader_program, "texture1"), 0);
            // TODO: Fill with other data like time, or recorded data
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, buffer_size, num_channels, GL_RED, GL_FLOAT, &render_stages[0].audio_buffer.data()[0]);
        }
            
        // Bind the Draw the triangles
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // unset the vertex array
        glBindVertexArray(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Read the data back from the GPU
    glBindFramebuffer(GL_FRAMEBUFFER, FBO[(num_stages-1) % 2]);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, PBO);
    glReadPixels(0, 0, buffer_size, num_channels, GL_RED, GL_FLOAT, 0);
    float * ptr2 = (float *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    if (ptr2) {
        memcpy(&output_buffer_data[0], ptr2, buffer_size * num_channels * sizeof(float));
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    // Output to screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Unbind everything
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    glutTimerFunc(0, render_callback, 0);
}

void AudioRenderer::main_loop()
{
    glutMainLoop();
}

bool AudioRenderer::cleanup()
{
    // Delete the shader programs
    for (int i = 0; i < num_stages; i++) {
        glDeleteProgram(render_stages[i].shader_program);
    }

    // Delete the textures
    glDeleteTextures(3, audio_texture);

    // Delete the framebuffers
    glDeleteFramebuffers(3, FBO);

    // Delete the vertex array and buffer
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

    // Delete the pixel buffer object
    glDeleteBuffers(1, &PBO);

    return true;
}

bool AudioRenderer::terminate()
{
    // Terminate the OpenGL context
    glutDestroyWindow(glutGetWindow());

    return true;
}

AudioRenderer::~AudioRenderer()
{
    // Clean up the OpenGL context
    if (!cleanup()) {
        std::cerr << "Failed to clean up OpenGL context." << std::endl;
    }
}