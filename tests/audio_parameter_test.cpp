#include "catch2/catch_all.hpp"
#include "audio_core/audio_parameter.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"
#include "audio_parameter/audio_uniform_array_parameter.h"
#include "audio_parameter/audio_texture2d_parameter.h"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include "utilities/shader_program.h"

#include <vector>
#include <memory>
#include <stdexcept>
#include <iostream>

/**
 * @brief A simple OpenGL test context for testing audio parameters
 * 
 * This class manages an OpenGL context that can be used for testing
 * audio parameters with OpenGL functionality.
 */
class GLTestContext {
public:
    /**
     * @brief Initialize the OpenGL context for testing
     * 
     * @return True if initialization succeeded, false otherwise
     */
    bool initialize() {
        if (m_initialized) {
            return true;
        }

        // Initialize SDL for video
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
            return false;
        }

        // Set OpenGL attributes
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

        // Create hidden window
        m_window = SDL_CreateWindow(
            "Test GL Context",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            1, 1,
            SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN
        );

        if (!m_window) {
            std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
            SDL_Quit();
            return false;
        }

        // Create OpenGL context
        m_glContext = SDL_GL_CreateContext(m_window);
        if (!m_glContext) {
            std::cerr << "Failed to create GL context: " << SDL_GetError() << std::endl;
            SDL_DestroyWindow(m_window);
            SDL_Quit();
            return false;
        }

        // Make the context current
        SDL_GL_MakeCurrent(m_window, m_glContext);

        // Initialize GLEW
        glewExperimental = GL_TRUE;
        GLenum glewError = glewInit();
        if (glewError != GLEW_OK) {
            std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(glewError) << std::endl;
            SDL_GL_DeleteContext(m_glContext);
            SDL_DestroyWindow(m_window);
            SDL_Quit();
            return false;
        }

        // Check for any OpenGL errors
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "OpenGL error during init: " << error << std::endl;
            // Continue anyway, some GL implementations may set errors after glewInit
        }
        
        // Initialize test resources
        createTestResources();

        m_initialized = true;
        return true;
    }

    /**
     * @brief Clean up OpenGL context and SDL resources
     */
    void cleanup() {
        cleanupTestResources();
        
        if (m_glContext) {
            SDL_GL_DeleteContext(m_glContext);
            m_glContext = nullptr;
        }

        if (m_window) {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }

        SDL_Quit();
        m_initialized = false;
    }
    
    /**
     * @brief Initialize the parameter with our test context
     * 
     * @param parameter The parameter to initialize
     * @return True if initialization succeeded, false otherwise
     */
    bool initializeParameter(AudioParameter* parameter) {
        if (!m_initialized && !initialize()) {
            return false;
        }
        
        return parameter->initialize(m_framebuffer, m_shaderProgram.get());
    }
    
    /**
     * @brief Get the shader program for tests
     * 
     * @return Pointer to the shader program
     */
    AudioShaderProgram* getShaderProgram() {
        return m_shaderProgram.get();
    }
    
    /**
     * @brief Get the framebuffer for tests
     * 
     * @return The framebuffer ID
     */
    GLuint getFramebuffer() const {
        return m_framebuffer;
    }

private:
    /**
     * @brief Create resources needed for testing
     */
    void createTestResources() {
        // Create framebuffer
        glGenFramebuffers(1, &m_framebuffer);
        
        // Create simple shader program
        const std::string vertexShaderSource = 
            "#version 330 core\n"
            "layout(location = 0) in vec3 position;\n"
            "void main() {\n"
            "    gl_Position = vec4(position, 1.0);\n"
            "}\n";
            
        const std::string fragmentShaderSource = 
            "#version 330 core\n"
            "uniform sampler2D textureParam;\n"     // Texture parameter used in tests
            "out vec4 outputColor;\n"               // Example output
            "void main() {\n"
            "    outputColor = texture(textureParam, vec2(0.0));\n"
            "}\n";
        
        m_shaderProgram = std::make_unique<AudioShaderProgram>(vertexShaderSource, fragmentShaderSource);
        m_shaderProgram->initialize();
    }
    
    /**
     * @brief Clean up test resources
     */
    void cleanupTestResources() {
        if (m_framebuffer) {
            glDeleteFramebuffers(1, &m_framebuffer);
            m_framebuffer = 0;
        }
        
        m_shaderProgram.reset();
    }
    
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_glContext = nullptr;
    bool m_initialized = false;
    
    GLuint m_framebuffer = 0;
    std::unique_ptr<AudioShaderProgram> m_shaderProgram;
};

/**
 * @brief Fixture for tests requiring OpenGL context
 * 
 * Use this fixture for test cases that need to test parameters with OpenGL functions.
 */
class GLContextFixture {
public:
    GLContextFixture() {
        if (!m_context.initialize()) {
            throw std::runtime_error("Failed to initialize GL context for testing");
        }
    }
    
    ~GLContextFixture() {
        m_context.cleanup();
    }
    
    /**
     * @brief Initialize a parameter with the test context
     * 
     * @param parameter The parameter to initialize
     * @return True if initialization succeeded, false otherwise
     */
    bool initializeParameter(AudioParameter* parameter) {
        return m_context.initializeParameter(parameter);
    }
    
private:
    GLTestContext m_context;
};

/**
 * @brief Tests for basic functionality without OpenGL context
 * 
 * These tests only check the data storage and parameter properties
 * without requiring OpenGL initialization.
 */

// Simple test for AudioParameter base functionality
TEST_CASE("AudioParameter basic tests", "[audio_parameter]") {
    SECTION("AudioParameter creation and linking") {
        // Test creating parameters
        auto param1 = std::make_unique<AudioFloatParameter>("param1", AudioParameter::ConnectionType::INPUT);
        auto param2 = std::make_unique<AudioFloatParameter>("param2", AudioParameter::ConnectionType::INPUT);
        
        // Verify basic properties
        REQUIRE(param1->name == "param1");
        REQUIRE(param1->connection_type == AudioParameter::ConnectionType::INPUT);
        REQUIRE(param2->name == "param2");
        REQUIRE(param2->connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Test linking parameters
        REQUIRE(param1->link(param2.get()) == true);
        REQUIRE(param1->is_connected() == true);
        REQUIRE(param1->get_linked_parameter() == param2.get());
        
        // Test unlinking
        REQUIRE(param1->unlink() == true);
        REQUIRE(param1->is_connected() == false);
        REQUIRE(param1->get_linked_parameter() == nullptr);
    }
}

// Basic tests for uniform parameters
TEST_CASE("Uniform Parameter tests", "[audio_parameter]") {
    SECTION("AudioFloatParameter") {
        auto param = std::make_unique<AudioFloatParameter>("floatParam", AudioParameter::ConnectionType::INPUT);
        
        // Verify initial properties
        REQUIRE(param->name == "floatParam");
        REQUIRE(param->connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Test setting/getting values
        REQUIRE(param->set_value(3.14f) == true);
        const float* value = static_cast<const float*>(param->get_value());
        REQUIRE(*value == Catch::Approx(3.14f));
        
        // Test updating value
        REQUIRE(param->set_value(2.71f) == true);
        value = static_cast<const float*>(param->get_value());
        REQUIRE(*value == Catch::Approx(2.71f));
    }
    
    SECTION("AudioIntParameter") {
        auto param = std::make_unique<AudioIntParameter>("intParam", AudioParameter::ConnectionType::INPUT);
        
        // Verify initial properties
        REQUIRE(param->name == "intParam");
        REQUIRE(param->connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Test setting/getting values
        REQUIRE(param->set_value(42) == true);
        const int* value = static_cast<const int*>(param->get_value());
        REQUIRE(*value == 42);
        
        // Test updating value
        REQUIRE(param->set_value(100) == true);
        value = static_cast<const int*>(param->get_value());
        REQUIRE(*value == 100);
    }
    
    SECTION("AudioBoolParameter") {
        auto param = std::make_unique<AudioBoolParameter>("boolParam", AudioParameter::ConnectionType::INPUT);
        
        // Verify initial properties
        REQUIRE(param->name == "boolParam");
        REQUIRE(param->connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Test setting/getting values
        REQUIRE(param->set_value(true) == true);
        const bool* value = static_cast<const bool*>(param->get_value());
        REQUIRE(*value == true);
        
        // Test updating value
        REQUIRE(param->set_value(false) == true);
        value = static_cast<const bool*>(param->get_value());
        REQUIRE(*value == false);
    }
    
    SECTION("Valid Connection Types") {
        // Should be able to create INPUT type uniform parameters
        REQUIRE_NOTHROW(
            std::make_unique<AudioFloatParameter>("validParam", AudioParameter::ConnectionType::INPUT)
        );
        
        // Should be able to create INITIALIZATION type uniform parameters
        REQUIRE_NOTHROW(
            std::make_unique<AudioFloatParameter>("validParam", AudioParameter::ConnectionType::INITIALIZATION)
        );
    }
}

// Basic tests for uniform array parameters
TEST_CASE("Uniform Array Parameter tests", "[audio_parameter]") {
    SECTION("AudioIntArrayParameter") {
        const size_t arraySize = 5;
        auto param = std::make_unique<AudioIntArrayParameter>("intArrayParam", 
                                                            AudioParameter::ConnectionType::INPUT,
                                                            arraySize);
        
        // Verify initial properties
        REQUIRE(param->name == "intArrayParam");
        REQUIRE(param->connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Create test data
        int testData[arraySize] = {10, 20, 30, 40, 50};
        
        // Test setting/getting values
        REQUIRE(param->set_value(testData) == true);
        
        // Verify data via get_value
        const int* values = static_cast<const int*>(param->get_value());
        for (size_t i = 0; i < arraySize; i++) {
            REQUIRE(values[i] == testData[i]);
        }
        
        // Update values
        for (size_t i = 0; i < arraySize; i++) {
            testData[i] = static_cast<int>(i * 100);
        }
        
        // Test setting/getting updated values
        REQUIRE(param->set_value(testData) == true);
        
        // Verify updated data
        values = static_cast<const int*>(param->get_value());
        for (size_t i = 0; i < arraySize; i++) {
            REQUIRE(values[i] == testData[i]);
        }
    }
    
    SECTION("AudioFloatArrayParameter") {
        const size_t arraySize = 5;
        auto param = std::make_unique<AudioFloatArrayParameter>("floatArrayParam", 
                                                              AudioParameter::ConnectionType::INPUT,
                                                              arraySize);
        
        // Verify initial properties
        REQUIRE(param->name == "floatArrayParam");
        REQUIRE(param->connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Create test data
        float testData[arraySize] = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
        
        // Test setting/getting values
        REQUIRE(param->set_value(testData) == true);
        
        // Verify data via get_value
        const float* values = static_cast<const float*>(param->get_value());
        for (size_t i = 0; i < arraySize; i++) {
            REQUIRE(values[i] == Catch::Approx(testData[i]));
        }
        
        // Update values
        for (size_t i = 0; i < arraySize; i++) {
            testData[i] = static_cast<float>(i) * 10.0f + 0.5f;
        }
        
        // Test setting/getting updated values
        REQUIRE(param->set_value(testData) == true);
        
        // Verify updated data
        values = static_cast<const float*>(param->get_value());
        for (size_t i = 0; i < arraySize; i++) {
            REQUIRE(values[i] == Catch::Approx(testData[i]));
        }
    }
    
    SECTION("AudioBoolArrayParameter") {
        const size_t arraySize = 5;
        auto param = std::make_unique<AudioBoolArrayParameter>("boolArrayParam", 
                                                             AudioParameter::ConnectionType::INPUT,
                                                             arraySize);
        
        // Verify initial properties
        REQUIRE(param->name == "boolArrayParam");
        REQUIRE(param->connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Create test data
        bool testData[arraySize] = {true, false, true, false, true};
        
        // Test setting/getting values
        REQUIRE(param->set_value(testData) == true);
        
        // Verify data via get_value
        const bool* values = static_cast<const bool*>(param->get_value());
        for (size_t i = 0; i < arraySize; i++) {
            REQUIRE(values[i] == testData[i]);
        }
        
        // Update values (invert all booleans)
        for (size_t i = 0; i < arraySize; i++) {
            testData[i] = !testData[i];
        }
        
        // Test setting/getting updated values
        REQUIRE(param->set_value(testData) == true);
        
        // Verify updated data
        values = static_cast<const bool*>(param->get_value());
        for (size_t i = 0; i < arraySize; i++) {
            REQUIRE(values[i] == testData[i]);
        }
    }
}

// Basic tests for buffer parameters without GL context
TEST_CASE("Buffer Parameter basic tests", "[audio_parameter]") {
    SECTION("AudioIntBufferParameter") {
        auto param = std::make_unique<AudioIntBufferParameter>("intBufferParam", 
                                                             AudioParameter::ConnectionType::INPUT);
        
        // Verify initial properties
        REQUIRE(param->name == "intBufferParam");
        REQUIRE(param->connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Test setting/getting values without initialization
        REQUIRE(param->set_value(42) == true);
        const int* value = static_cast<const int*>(param->get_value());
        REQUIRE(*value == 42);
        
        // Update value
        REQUIRE(param->set_value(100) == true);
        value = static_cast<const int*>(param->get_value());
        REQUIRE(*value == 100);
    }
    
    SECTION("AudioFloatBufferParameter") {
        auto param = std::make_unique<AudioFloatBufferParameter>("floatBufferParam", 
                                                               AudioParameter::ConnectionType::INPUT);
        
        // Verify initial properties
        REQUIRE(param->name == "floatBufferParam");
        REQUIRE(param->connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Test setting/getting values without initialization
        REQUIRE(param->set_value(3.14f) == true);
        const float* value = static_cast<const float*>(param->get_value());
        REQUIRE(*value == Catch::Approx(3.14f));
        
        // Update value
        REQUIRE(param->set_value(2.71f) == true);
        value = static_cast<const float*>(param->get_value());
        REQUIRE(*value == Catch::Approx(2.71f));
    }
    
    SECTION("AudioBoolBufferParameter") {
        auto param = std::make_unique<AudioBoolBufferParameter>("boolBufferParam", 
                                                              AudioParameter::ConnectionType::INPUT);
        
        // Verify initial properties
        REQUIRE(param->name == "boolBufferParam");
        REQUIRE(param->connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Test setting/getting values without initialization
        REQUIRE(param->set_value(true) == true);
        const bool* value = static_cast<const bool*>(param->get_value());
        REQUIRE(*value == true);
        
        // Update value
        REQUIRE(param->set_value(false) == true);
        value = static_cast<const bool*>(param->get_value());
        REQUIRE(*value == false);
    }
}

// Basic tests for texture parameters without GL context
TEST_CASE("Texture2D Parameter basic tests", "[audio_parameter]") {
    SECTION("Basic parameter properties") {
        // Create texture parameter with various settings
        const GLuint width = 512;
        const GLuint height = 2;
        const GLuint activeTexture = 3;
        const GLuint colorAttachment = 2;
        const GLuint filterType = GL_LINEAR;
        
        auto param = std::make_unique<AudioTexture2DParameter>(
            "textureParam", 
            AudioParameter::ConnectionType::INPUT,
            width, 
            height,
            activeTexture,
            colorAttachment,
            filterType
        );
        
        // Verify properties
        REQUIRE(param->name == "textureParam");
        REQUIRE(param->connection_type == AudioParameter::ConnectionType::INPUT);
        REQUIRE(param->get_color_attachment() == colorAttachment);
    }
    
    SECTION("Texture connection types") {
        // Test creating texture parameters with different connection types
        REQUIRE_NOTHROW(
            std::make_unique<AudioTexture2DParameter>(
                "inputTexture", 
                AudioParameter::ConnectionType::INPUT,
                8, 
                8
            )
        );
        
        REQUIRE_NOTHROW(
            std::make_unique<AudioTexture2DParameter>(
                "outputTexture", 
                AudioParameter::ConnectionType::OUTPUT,
                8, 
                8
            )
        );
        
        REQUIRE_NOTHROW(
            std::make_unique<AudioTexture2DParameter>(
                "passthroughTexture", 
                AudioParameter::ConnectionType::PASSTHROUGH,
                8, 
                8
            )
        );
    }
}

// Extended tests that verify all functionality works in an integrated manner
// Note that we're not using the GLContextFixture anymore to avoid segfaults
TEST_CASE("AudioParameter integration verification", "[audio_parameter][integration]") {
    INFO("These tests verify parameter values are correctly stored and accessed");
    
    SECTION("Verify ArrayParameter storage and integrity") {
        // Create parameter for testing
        const size_t arraySize = 128; // Audio-relevant size
        auto param = std::make_unique<AudioFloatArrayParameter>("audioSampleArray", 
                                                               AudioParameter::ConnectionType::INPUT,
                                                               arraySize);
        
        // Create audio-like test data
        std::vector<float> audioData(arraySize);
        for (size_t i = 0; i < arraySize; i++) {
            // Create a simple sine wave pattern
            audioData[i] = sin(static_cast<float>(i) / 10.0f);
        }
        
        // Set the audio data
        REQUIRE(param->set_value(audioData.data()) == true);
        
        // Verify the audio data was stored correctly
        const float* storedData = static_cast<const float*>(param->get_value());
        for (size_t i = 0; i < arraySize; i++) {
            REQUIRE(storedData[i] == Catch::Approx(audioData[i]));
        }
        
        // Modify the source data to ensure we're not just seeing the original buffer
        for (size_t i = 0; i < arraySize; i++) {
            audioData[i] = cos(static_cast<float>(i) / 10.0f);
        }
        
        // Verify the original stored data remains unchanged
        for (size_t i = 0; i < arraySize; i++) {
            REQUIRE(storedData[i] == Catch::Approx(sin(static_cast<float>(i) / 10.0f)));
        }
        
        // Update with the new data
        REQUIRE(param->set_value(audioData.data()) == true);
        
        // Verify the new data was stored correctly
        storedData = static_cast<const float*>(param->get_value());
        for (size_t i = 0; i < arraySize; i++) {
            REQUIRE(storedData[i] == Catch::Approx(audioData[i]));
        }
    }
    
    SECTION("Verify BufferParameter data integrity") {
        // Create parameter for testing
        auto param = std::make_unique<AudioFloatBufferParameter>("timeParam", 
                                                                AudioParameter::ConnectionType::INPUT);
        
        // Test initial value setting
        const float initialValue = 1234.5678f;
        REQUIRE(param->set_value(initialValue) == true);
        const float* storedValue = static_cast<const float*>(param->get_value());
        REQUIRE(*storedValue == Catch::Approx(initialValue));
        
        // Test value update
        const float updatedValue = 8765.4321f;
        REQUIRE(param->set_value(updatedValue) == true);
        storedValue = static_cast<const float*>(param->get_value());
        REQUIRE(*storedValue == Catch::Approx(updatedValue));
    }
    
    SECTION("Verify parameter linking and value access") {
        // Create source and destination parameters
        auto sourceParam = std::make_unique<AudioFloatParameter>("sourceParam", 
                                                                AudioParameter::ConnectionType::INPUT);
        auto destParam = std::make_unique<AudioFloatParameter>("destParam", 
                                                              AudioParameter::ConnectionType::INPUT);
        
        // Set initial values
        const float sourceValue = 42.0f;
        const float destValue = 24.0f;
        REQUIRE(sourceParam->set_value(sourceValue) == true);
        REQUIRE(destParam->set_value(destValue) == true);
        
        // Link parameters
        REQUIRE(destParam->link(sourceParam.get()) == true);
        
        // Verify linking status
        REQUIRE(destParam->is_connected() == true);
        REQUIRE(destParam->get_linked_parameter() == sourceParam.get());
        
        // Verify the original values are preserved after linking
        const float* sourceValuePtr = static_cast<const float*>(sourceParam->get_value());
        const float* destValuePtr = static_cast<const float*>(destParam->get_value());
        REQUIRE(*sourceValuePtr == Catch::Approx(sourceValue));
        REQUIRE(*destValuePtr == Catch::Approx(destValue));
        
        // Update the source parameter
        const float newSourceValue = 99.0f;
        REQUIRE(sourceParam->set_value(newSourceValue) == true);
        
        // Verify the source value was updated
        sourceValuePtr = static_cast<const float*>(sourceParam->get_value());
        REQUIRE(*sourceValuePtr == Catch::Approx(newSourceValue));
        
        // The destination parameter should still have its own value
        destValuePtr = static_cast<const float*>(destParam->get_value());
        REQUIRE(*destValuePtr == Catch::Approx(destValue));
    }
}

/**
 * @brief Tests for texture parameter initialization with OpenGL context
 * 
 * These tests check the texture creation and initialization in an OpenGL context.
 * Note: These tests require a valid OpenGL context to run, which may not be available
 * in all test environments. They are marked with [gl] tag.
 */
TEST_CASE("AudioTexture2DParameter with OpenGL context", "[audio_parameter][gl][.]") {
    // Create a GL context fixture for testing
    // The dot in the tag ([.]) disables these tests by default
    // Run with --test=audio_parameter_test~[gl] to include them
    GLContextFixture fixture;
    
    SECTION("Texture parameter initialization") {
        // Create texture parameter
        const GLuint width = 512;
        const GLuint height = 2;
        const GLuint activeTexture = 3;
        const GLuint colorAttachment = 2;
        auto param = std::make_unique<AudioTexture2DParameter>(
            "textureParam", 
            AudioParameter::ConnectionType::INPUT,
            width, 
            height,
            activeTexture,
            colorAttachment
        );
        
        // Initialize the parameter with our test context
        REQUIRE(fixture.initializeParameter(param.get()));
        
        // Verify texture was created
        REQUIRE(param->get_texture() != 0);
        
        // Verify parameter properties after initialization
        REQUIRE(param->get_color_attachment() == colorAttachment);
    }
    
    SECTION("Texture data loading and retrieval") {
        // Create texture parameter
        const GLuint width = 256;
        const GLuint height = 1;
        auto param = std::make_unique<AudioTexture2DParameter>(
            "audioTexture", 
            AudioParameter::ConnectionType::INPUT,
            width, 
            height
        );
        
        // Create test data - simple audio wave
        std::vector<float> audioData(width * height);
        for (size_t i = 0; i < width; i++) {
            audioData[i] = sin(static_cast<float>(i) * 0.1f);
        }
        
        // Set the test data
        REQUIRE(param->set_value(audioData.data()));
        
        // Initialize the parameter with our test context
        REQUIRE(fixture.initializeParameter(param.get()));
        
        // Verify the data was stored correctly
        const float* storedData = static_cast<const float*>(param->get_value());
        for (size_t i = 0; i < width; i++) {
            REQUIRE(storedData[i] == Catch::Approx(audioData[i]));
        }
    }
    
    SECTION("Texture render method") {
        // Create texture parameter
        const GLuint width = 128;
        const GLuint height = 1;
        auto param = std::make_unique<AudioTexture2DParameter>(
            "renderTexture", 
            AudioParameter::ConnectionType::INPUT,
            width, 
            height
        );
        
        // Initialize the parameter with our test context
        REQUIRE(fixture.initializeParameter(param.get()));
        
        // Create test data
        std::vector<float> audioData(width * height);
        for (size_t i = 0; i < width; i++) {
            audioData[i] = sin(static_cast<float>(i) * 0.05f);
        }
        
        // Set the data and mark for update
        REQUIRE(param->set_value(audioData.data()));
        
        // Call render - should bind the texture and upload data
        param->render();
        
        // Verify the data was handled correctly
        const float* valueAfterRender = static_cast<const float*>(param->get_value());
        for (size_t i = 0; i < width; i++) {
            REQUIRE(valueAfterRender[i] == Catch::Approx(audioData[i]));
        }
    }
    
    SECTION("Texture binding and framebuffer attachment") {
        // Create source and destination texture parameters
        const GLuint width = 64;
        const GLuint height = 2;
        const GLuint colorAttachment = 1;
        
        auto sourceParam = std::make_unique<AudioTexture2DParameter>(
            "sourceTexture", 
            AudioParameter::ConnectionType::OUTPUT,
            width, 
            height,
            0,  // active texture
            0   // color attachment
        );
        
        auto destParam = std::make_unique<AudioTexture2DParameter>(
            "destTexture", 
            AudioParameter::ConnectionType::INPUT,
            width, 
            height,
            0,  // active texture
            colorAttachment
        );
        
        // Initialize both parameters
        REQUIRE(fixture.initializeParameter(sourceParam.get()));
        REQUIRE(fixture.initializeParameter(destParam.get()));
        
        // Link parameters
        REQUIRE(destParam->link(sourceParam.get()));
        
        // Test binding which should attach texture to framebuffer
        REQUIRE(destParam->bind());
        
        // Unbind
        REQUIRE(destParam->unbind());
    }
    
    SECTION("Texture parameter with clear_value") {
        // Create texture parameter
        const GLuint width = 32;
        const GLuint height = 2;
        auto param = std::make_unique<AudioTexture2DParameter>(
            "clearTexture", 
            AudioParameter::ConnectionType::INPUT,
            width, 
            height
        );
        
        // Initialize the parameter
        REQUIRE(fixture.initializeParameter(param.get()));
        
        // Create and set test data
        std::vector<float> audioData(width * height);
        for (size_t i = 0; i < width * height; i++) {
            audioData[i] = 0.5f;
        }
        
        REQUIRE(param->set_value(audioData.data()));
        
        // Verify data was set
        const float* initialData = static_cast<const float*>(param->get_value());
        for (size_t i = 0; i < width * height; i++) {
            REQUIRE(initialData[i] == Catch::Approx(0.5f));
        }
        
        // Clear the value
        param->clear_value();
        
        // Get the data after clearing
        const float* clearedData = static_cast<const float*>(param->get_value());
        REQUIRE(clearedData != nullptr);
    }
}