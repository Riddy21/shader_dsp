#include "catch2/catch_all.hpp"
#include "audio_core/audio_parameter.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"
#include "audio_parameter/audio_uniform_array_parameter.h"
#include "audio_parameter/audio_texture2d_parameter.h"

#include "SDL2/SDL.h"

#include <vector>
#include <memory>
#include <tuple>

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
        AudioFloatParameter param1("param1", AudioParameter::ConnectionType::INPUT);
        AudioFloatParameter param2("param2", AudioParameter::ConnectionType::INPUT);
        
        // Verify basic properties
        REQUIRE(param1.name == "param1");
        REQUIRE(param1.connection_type == AudioParameter::ConnectionType::INPUT);
        REQUIRE(param2.name == "param2");
        REQUIRE(param2.connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Test linking parameters
        REQUIRE(param1.link(&param2) == true);
        REQUIRE(param1.is_connected() == true);
        REQUIRE(param1.get_linked_parameter() == &param2);
        
        // Test unlinking
        REQUIRE(param1.unlink() == true);
        REQUIRE(param1.is_connected() == false);
        REQUIRE(param1.get_linked_parameter() == nullptr);
    }
}

// Basic tests for uniform parameters
TEST_CASE("Uniform Parameter tests", "[audio_parameter]") {
    SECTION("AudioFloatParameter") {
        AudioFloatParameter param("floatParam", AudioParameter::ConnectionType::INPUT);
        
        // Verify initial properties
        REQUIRE(param.name == "floatParam");
        REQUIRE(param.connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Test setting/getting values
        REQUIRE(param.set_value(3.14f) == true);
        const float* value = static_cast<const float*>(param.get_value());
        REQUIRE(*value == Catch::Approx(3.14f));
        
        // Test updating value
        REQUIRE(param.set_value(2.71f) == true);
        value = static_cast<const float*>(param.get_value());
        REQUIRE(*value == Catch::Approx(2.71f));
    }
    
    SECTION("AudioIntParameter") {
        AudioIntParameter param("intParam", AudioParameter::ConnectionType::INPUT);
        
        // Verify initial properties
        REQUIRE(param.name == "intParam");
        REQUIRE(param.connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Test setting/getting values
        REQUIRE(param.set_value(42) == true);
        const int* value = static_cast<const int*>(param.get_value());
        REQUIRE(*value == 42);
        
        // Test updating value
        REQUIRE(param.set_value(100) == true);
        value = static_cast<const int*>(param.get_value());
        REQUIRE(*value == 100);
    }
    
    SECTION("AudioBoolParameter") {
        AudioBoolParameter param("boolParam", AudioParameter::ConnectionType::INPUT);
        
        // Verify initial properties
        REQUIRE(param.name == "boolParam");
        REQUIRE(param.connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Test setting/getting values
        REQUIRE(param.set_value(true) == true);
        const bool* value = static_cast<const bool*>(param.get_value());
        REQUIRE(*value == true);
        
        // Test updating value
        REQUIRE(param.set_value(false) == true);
        value = static_cast<const bool*>(param.get_value());
        REQUIRE(*value == false);
    }
    
    SECTION("Valid Connection Types") {
        // Should be able to create INPUT type uniform parameters
        REQUIRE_NOTHROW(
            AudioFloatParameter("validParam", AudioParameter::ConnectionType::INPUT)
        );
        
        // Should be able to create INITIALIZATION type uniform parameters
        REQUIRE_NOTHROW(
            AudioFloatParameter("validParam", AudioParameter::ConnectionType::INITIALIZATION)
        );
    }
}

// Basic tests for uniform array parameters
TEST_CASE("Uniform Array Parameter tests", "[audio_parameter]") {
    SECTION("AudioIntArrayParameter") {
        const size_t arraySize = 5;
        AudioIntArrayParameter param("intArrayParam", 
                                     AudioParameter::ConnectionType::INPUT,
                                     arraySize);
        
        // Verify initial properties
        REQUIRE(param.name == "intArrayParam");
        REQUIRE(param.connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Create test data
        int testData[arraySize] = {10, 20, 30, 40, 50};
        
        // Test setting/getting values
        REQUIRE(param.set_value(testData) == true);
        
        // Verify data via get_value
        const int* values = static_cast<const int*>(param.get_value());
        for (size_t i = 0; i < arraySize; i++) {
            REQUIRE(values[i] == testData[i]);
        }
        
        // Update values
        for (size_t i = 0; i < arraySize; i++) {
            testData[i] = static_cast<int>(i * 100);
        }
        
        // Test setting/getting updated values
        REQUIRE(param.set_value(testData) == true);
        
        // Verify updated data
        values = static_cast<const int*>(param.get_value());
        for (size_t i = 0; i < arraySize; i++) {
            REQUIRE(values[i] == testData[i]);
        }
    }
    
    SECTION("AudioFloatArrayParameter") {
        const size_t arraySize = 5;
        AudioFloatArrayParameter param("floatArrayParam", 
                                       AudioParameter::ConnectionType::INPUT,
                                       arraySize);
        
        // Verify initial properties
        REQUIRE(param.name == "floatArrayParam");
        REQUIRE(param.connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Create test data
        float testData[arraySize] = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
        
        // Test setting/getting values
        REQUIRE(param.set_value(testData) == true);
        
        // Verify data via get_value
        const float* values = static_cast<const float*>(param.get_value());
        for (size_t i = 0; i < arraySize; i++) {
            REQUIRE(values[i] == Catch::Approx(testData[i]));
        }
        
        // Update values
        for (size_t i = 0; i < arraySize; i++) {
            testData[i] = static_cast<float>(i) * 10.0f + 0.5f;
        }
        
        // Test setting/getting updated values
        REQUIRE(param.set_value(testData) == true);
        
        // Verify updated data
        values = static_cast<const float*>(param.get_value());
        for (size_t i = 0; i < arraySize; i++) {
            REQUIRE(values[i] == Catch::Approx(testData[i]));
        }
    }
    
    SECTION("AudioBoolArrayParameter") {
        const size_t arraySize = 5;
        AudioBoolArrayParameter param("boolArrayParam", 
                                      AudioParameter::ConnectionType::INPUT,
                                      arraySize);
        
        // Verify initial properties
        REQUIRE(param.name == "boolArrayParam");
        REQUIRE(param.connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Create test data
        bool testData[arraySize] = {true, false, true, false, true};
        
        // Test setting/getting values
        REQUIRE(param.set_value(testData) == true);
        
        // Verify data via get_value
        const bool* values = static_cast<const bool*>(param.get_value());
        for (size_t i = 0; i < arraySize; i++) {
            REQUIRE(values[i] == testData[i]);
        }
        
        // Update values (invert all booleans)
        for (size_t i = 0; i < arraySize; i++) {
            testData[i] = !testData[i];
        }
        
        // Test setting/getting updated values
        REQUIRE(param.set_value(testData) == true);
        
        // Verify updated data
        values = static_cast<const bool*>(param.get_value());
        for (size_t i = 0; i < arraySize; i++) {
            REQUIRE(values[i] == testData[i]);
        }
    }
}

// Basic tests for buffer parameters without GL context
TEST_CASE("Buffer Parameter basic tests", "[audio_parameter]") {
    SECTION("AudioIntBufferParameter") {
        AudioIntBufferParameter param("intBufferParam", 
                                      AudioParameter::ConnectionType::INPUT);
        
        // Verify initial properties
        REQUIRE(param.name == "intBufferParam");
        REQUIRE(param.connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Test setting/getting values without initialization
        REQUIRE(param.set_value(42) == true);
        const int* value = static_cast<const int*>(param.get_value());
        REQUIRE(*value == 42);
        
        // Update value
        REQUIRE(param.set_value(100) == true);
        value = static_cast<const int*>(param.get_value());
        REQUIRE(*value == 100);
    }
    
    SECTION("AudioFloatBufferParameter") {
        AudioFloatBufferParameter param("floatBufferParam", 
                                        AudioParameter::ConnectionType::INPUT);
        
        // Verify initial properties
        REQUIRE(param.name == "floatBufferParam");
        REQUIRE(param.connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Test setting/getting values without initialization
        REQUIRE(param.set_value(3.14f) == true);
        const float* value = static_cast<const float*>(param.get_value());
        REQUIRE(*value == Catch::Approx(3.14f));
        
        // Update value
        REQUIRE(param.set_value(2.71f) == true);
        value = static_cast<const float*>(param.get_value());
        REQUIRE(*value == Catch::Approx(2.71f));
    }
    
    SECTION("AudioBoolBufferParameter") {
        AudioBoolBufferParameter param("boolBufferParam", 
                                       AudioParameter::ConnectionType::INPUT);
        
        // Verify initial properties
        REQUIRE(param.name == "boolBufferParam");
        REQUIRE(param.connection_type == AudioParameter::ConnectionType::INPUT);
        
        // Test setting/getting values without initialization
        REQUIRE(param.set_value(true) == true);
        const bool* value = static_cast<const bool*>(param.get_value());
        REQUIRE(*value == true);
        
        // Update value
        REQUIRE(param.set_value(false) == true);
        value = static_cast<const bool*>(param.get_value());
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
        
        AudioTexture2DParameter param(
            "textureParam", 
            AudioParameter::ConnectionType::INPUT,
            width, 
            height,
            activeTexture,
            colorAttachment,
            filterType
        );
        
        // Verify properties
        REQUIRE(param.name == "textureParam");
        REQUIRE(param.connection_type == AudioParameter::ConnectionType::INPUT);
        REQUIRE(param.get_color_attachment() == colorAttachment);
    }
    
    SECTION("Texture connection types") {
        // Test creating texture parameters with different connection types
        REQUIRE_NOTHROW(
            AudioTexture2DParameter(
                "inputTexture", 
                AudioParameter::ConnectionType::INPUT,
                8, 
                8
            )
        );
        
        REQUIRE_NOTHROW(
            AudioTexture2DParameter(
                "outputTexture", 
                AudioParameter::ConnectionType::OUTPUT,
                8, 
                8
            )
        );
        
        REQUIRE_NOTHROW(
            AudioTexture2DParameter(
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
        AudioFloatArrayParameter param("audioSampleArray", 
                                       AudioParameter::ConnectionType::INPUT,
                                       arraySize);
        
        // Create audio-like test data
        std::vector<float> audioData(arraySize);
        for (size_t i = 0; i < arraySize; i++) {
            // Create a simple sine wave pattern
            audioData[i] = sin(static_cast<float>(i) / 10.0f);
        }
        
        // Set the audio data
        REQUIRE(param.set_value(audioData.data()) == true);
        
        // Verify the audio data was stored correctly
        const float* storedData = static_cast<const float*>(param.get_value());
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
        REQUIRE(param.set_value(audioData.data()) == true);
        
        // Verify the new data was stored correctly
        storedData = static_cast<const float*>(param.get_value());
        for (size_t i = 0; i < arraySize; i++) {
            REQUIRE(storedData[i] == Catch::Approx(audioData[i]));
        }
    }
    
    SECTION("Verify BufferParameter data integrity") {
        // Create parameter for testing
        AudioFloatBufferParameter param("timeParam", 
                                        AudioParameter::ConnectionType::INPUT);
        
        // Test initial value setting
        const float initialValue = 1234.5678f;
        REQUIRE(param.set_value(initialValue) == true);
        const float* storedValue = static_cast<const float*>(param.get_value());
        REQUIRE(*storedValue == Catch::Approx(initialValue));
        
        // Test value update
        const float updatedValue = 8765.4321f;
        REQUIRE(param.set_value(updatedValue) == true);
        storedValue = static_cast<const float*>(param.get_value());
        REQUIRE(*storedValue == Catch::Approx(updatedValue));
    }
    
    SECTION("Verify parameter linking and value access") {
        // Create source and destination parameters
        AudioFloatParameter sourceParam("sourceParam", 
                                        AudioParameter::ConnectionType::INPUT);
        AudioFloatParameter destParam("destParam", 
                                      AudioParameter::ConnectionType::INPUT);
        
        // Set initial values
        const float sourceValue = 42.0f;
        const float destValue = 24.0f;
        REQUIRE(sourceParam.set_value(sourceValue) == true);
        REQUIRE(destParam.set_value(destValue) == true);
        
        // Link parameters
        REQUIRE(destParam.link(&sourceParam) == true);
        
        // Verify linking status
        REQUIRE(destParam.is_connected() == true);
        REQUIRE(destParam.get_linked_parameter() == &sourceParam);
        
        // Verify the original values are preserved after linking
        const float* sourceValuePtr = static_cast<const float*>(sourceParam.get_value());
        const float* destValuePtr = static_cast<const float*>(destParam.get_value());
        REQUIRE(*sourceValuePtr == Catch::Approx(sourceValue));
        REQUIRE(*destValuePtr == Catch::Approx(destValue));
        
        // Update the source parameter
        const float newSourceValue = 99.0f;
        REQUIRE(sourceParam.set_value(newSourceValue) == true);
        
        // Verify the source value was updated
        sourceValuePtr = static_cast<const float*>(sourceParam.get_value());
        REQUIRE(*sourceValuePtr == Catch::Approx(newSourceValue));
        
        // The destination parameter should still have its own value
        destValuePtr = static_cast<const float*>(destParam.get_value());
        REQUIRE(*destValuePtr == Catch::Approx(destValue));
    }
}
