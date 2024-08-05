#include <GL/glew.h>
#include <GL/glut.h>
#include <iostream>
#include <vector>
#include <math.h>
#include <portaudio.h>
#include <cstring>
#include <sstream>
#include <iomanip>

// Add a mutex to prevent the GPU from reading the data while it's being written
#include <mutex>

// define mutex
std::mutex mtx;

const GLchar* vertexShaderSource = R"glsl(
    #version 300 es
    precision highp float;
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in float aTexCoord;
    out float TexCoord;
    void main()
    {
        gl_Position = vec4(aPos, 0.0, 1.0);
        TexCoord = aTexCoord;
    }
)glsl";

// Define variables for FPS calculation
int frameCount = 0;
int currentTime = 0;
int previousTime = 0;

float * ptr;
float * ptr2;
void calculateFPS() {
    int elapsedTime = glutGet(GLUT_ELAPSED_TIME);
    int deltaTime = elapsedTime - previousTime;

    if (deltaTime > 1000) {  // Update every second
        float fps = static_cast<float>(frameCount) / (deltaTime / 1000.0f);
        std::stringstream ss;
        ss << "FPS: " << std::fixed << std::setprecision(2) << fps;
        printf("FPS: %f\n", fps);
        // print ptr2 data
        printf("data2 = [");
        for (int i = 0; i < 400; i++) {
            printf("%f, ", ptr2[i]);
        }
        printf("]\n");
        printf("data1 = [");
        for (int i = 0; i < 400; i++) {
            printf("%f, ", ptr[i]);
        }
        printf("]\n");
        glutSetWindowTitle(ss.str().c_str());

        previousTime = elapsedTime;
        frameCount = 0;
    }
}

const GLchar* fragmentShaderSource[4];
GLint vertexShader[4], fragmentShader[4], shaderProgram[4];
GLuint VBO, VAO, PBO[2], FBO[2];
GLuint texture[2];

PaStream *stream;

GLuint compileShader(GLenum type, const GLchar* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    return shader;
}

GLuint createShaderProgram(GLuint vertexShader, GLuint fragmentShader) {
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    GLint success;
    GLchar infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    return shaderProgram;
}


void display(int value) {
    calculateFPS();
    frameCount++;
    ptr = (float *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    if (ptr) {
        for (int i = 0; i < 400; i++) {
            ptr[i] = (float)((double) cos(((double)i/(double)400)* 3.14159265f * 4.0f))*0.5f + 0.5f;
        }
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    }

    // Update texture with PBO data
    glBindTexture(GL_TEXTURE_2D, texture[0]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 400, 1, GL_RED, GL_FLOAT, 0);

    // Clear the screen
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Now you can switch between FBOs for each post-processing step
    for (int i = 0; i < 3; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, FBO[i % 2]);
        // Bind the texture from the previous step
        glUseProgram(shaderProgram[i]);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, texture[i % 2]);
    }

    // NOTE: Must output from frame buffer to avoid precision loss
    // mutex lock this section
    mtx.lock();

    // Read the data back from the GPU
    //glBindBuffer(GL_PIXEL_PACK_BUFFER, PBO[1]);
    glReadPixels(0, 0, 400, 1, GL_RED, GL_FLOAT, 0);

    ptr2 = (float *)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    if (ptr2) {
        for (int i = 0; i < 400; i++) {
            ptr2[i] = (ptr2[i] - 0.5f ) * 2.0f;
        }
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }

    mtx.unlock();


    // Second shader processing step
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(shaderProgram[3]);
    glBindTexture(GL_TEXTURE_1D, texture[2%2]);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // TODO: I think we need some kind of mutex here so it doesn't update when it's being read
    glutSwapBuffers(); // if you want to unbound the frame rate, don't need to display on screen
    glutTimerFunc(0, display, 0);
}

#define NUM_SECONDS   (5)
#define SAMPLE_RATE   (44100)
#define FRAMES_PER_BUFFER  (400)

static int patestCallback(const void *inputBuffer, void *outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo *timeInfo,
    PaStreamCallbackFlags statusFlags,
    void *userData) {
    // Assign the userData to the output buffer
    float *out = (float *)outputBuffer;
    float *ptr2 = (float *)userData;
    // Assign the userData to the input buffer
    mtx.lock();
    std::memcpy(out, ptr2, framesPerBuffer*sizeof(float));
    mtx.unlock();
    (void)inputBuffer; /* Prevent unused variable warning. */
    (void)timeInfo;
    (void)statusFlags;
    return paContinue;
}

void init_audio() {
    // Initialize the audio
    PaStreamParameters outputParameters;
    PaError err;
    
    printf("PortAudio Test: output sine wave. SR = %d, BufSize = %d\n", SAMPLE_RATE, FRAMES_PER_BUFFER);

    err = Pa_Initialize();
    if (err != paNoError) goto error;

    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice) {
        fprintf(stderr, "Error: No default output device.\n");
        goto error;
    }
    outputParameters.channelCount = 1;       /* mono output */
    outputParameters.sampleFormat = paFloat32; /* 32 bit floating point output */
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
        &stream,
        NULL, /* no input */
        &outputParameters,
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        paClipOff,      /* we won't output out of range samples so don't bother clipping them */
        patestCallback,
        ptr2); /* no callback, so no callback userData */

    if (err != paNoError) goto error;

    err = Pa_StartStream(stream);
    if (err != paNoError) goto error;

    return ;

error:
    Pa_Terminate();
    fprintf(stderr, "An error occured while using the portaudio stream\n");
    fprintf(stderr, "Error number: %d\n", err);
    fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
}

void idle() {
    glutPostRedisplay();
}

int main(int argc, char** argv) {
    fragmentShaderSource[0] = R"glsl(
        #version 300 es
        precision highp float;
        in float TexCoord;
        uniform sampler2D audioData;
        out vec4 FragColor;
        void main()
        {
            float color = texture(audioData, vec2(TexCoord, 0.5)).r;
            FragColor = vec4(color, 0.0, 0.0, 1.0);
        }
    )glsl";

    fragmentShaderSource[1] = R"glsl(
        #version 300 es
        precision highp float;
        in float TexCoord;
        uniform sampler2D audioData;
        out vec4 FragColor;
        void main()
        {
            float color = texture(audioData, vec2(TexCoord, 0.5)).r;
            FragColor = vec4(color, 0.0, color, 1.0);
        }
    )glsl";

    fragmentShaderSource[2] = R"glsl(
        #version 300 es
        precision highp float;
        in float TexCoord;
        uniform sampler2D audioData;
        out vec4 FragColor;
        void main()
        {
            float color = texture(audioData, vec2(TexCoord, 0.5)).r;
            FragColor = vec4(color, color, 0.0, 1.0);
        }
    )glsl";

    fragmentShaderSource[3] = R"glsl(
        #version 300 es
        precision highp float;
        in float TexCoord;
        uniform sampler2D audioData;
        out vec4 FragColor;
        void main()
        {
            float color = texture(audioData, vec2(TexCoord, 0.5)).r;
            FragColor = vec4(color, color, color, 1.0);
        }
    )glsl";

    // Intialize the OpenGL context
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_SINGLE);
    glutInitWindowSize(400, 200);
    glutCreateWindow("Audio Processing");

    // After creating the OpenGL context...
    const GLubyte* glVersion = glGetString(GL_VERSION);
    const GLubyte* glslVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);
    
    std::cout << "OpenGL Version: " << glVersion << std::endl;
    std::cout << "GLSL Version: " << glslVersion << std::endl;
    
    // Intialize GLEW
    glewInit();

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);

    // Compile the vertex and fragment shaders
    for (int i = 0; i < 4; i++) {
        vertexShader[i] = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
        fragmentShader[i] = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource[i]);
        shaderProgram[i] = createShaderProgram(vertexShader[i], fragmentShader[i]);
        glDeleteShader(vertexShader[i]);
        glDeleteShader(fragmentShader[i]);
    }

    // Just a default set of vertices, triangle
    GLfloat vertices[] = {
        -1.0f, -1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,
        1.0f,  -1.0f, 1.0f,
        1.0f,  1.0f, 1.0f,
        -1.0f, 1.0f, 0.0f,
        1.0f,  -1.0f, 1.0f
    };

    // Generate texture for the PBO
    // Make the texture 2D
    glGenTextures(2, texture);
    glGenFramebuffers(2, FBO);

    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, FBO[i]);
        glBindTexture(GL_TEXTURE_2D, texture[i]);
        // Configuring the texture
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        float flatColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, flatColor);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[i], 0);

        // Allocate memory for the texture
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 400, 1, 0, GL_RED, GL_FLOAT, nullptr);
    }

    // OpenGL needs to know how to interpret the vertex data
    // Stores pointers to more than 1 VBO
    // VAO is an object that stores the vertex attribute configurations
    // Generate the VBO and VAO with only 1 object each
    glGenVertexArrays(1, &VAO);
    // VBO is a buffer object that stores the vertices data
    // Send data in batches
    // Generate the VBO with only 1 object
    glGenBuffers(1, &VBO);
    // Generate Pixel Buffer Object
    glGenBuffers(2, PBO);

    // Make the VAO the current Vertex Array Object by binding it
    glBindVertexArray(VAO); // Making a certain object the current object
    // Bind the VBO to the GL_ARRAY_BUFFER target
    glBindBuffer(GL_ARRAY_BUFFER, VBO); // Making a certain object the current object
    // Bind the PBO to the GL_UNPIXEL_PACK_BUFFER target
    // PIXEL_UNPACK_BUFFER is used to transfer data from the CPU to the GPU
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO[0]);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, PBO[1]);

    // Allocate the memory
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, 400*sizeof(float), nullptr, GL_STREAM_DRAW);
    glBufferData(GL_PIXEL_PACK_BUFFER, 400*sizeof(float), nullptr, GL_STREAM_READ);

    // Configure the vertex attributes
    // Communicate with a vertex shader from the outside
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    // Enable the vertex attribute
    glEnableVertexAttribArray(0);
    // Add the texture coord to the vertex attributes
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)(2 * sizeof(GLfloat)));
    // Enable the vertex attribute
    glEnableVertexAttribArray(1);

    // unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Main loop
    display(1);
    init_audio();
    glutTimerFunc(0, display, 0);// Run as fast as possible
    glutDisplayFunc(idle);
    glutMainLoop();

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

}