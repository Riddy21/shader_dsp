#pragma once
#ifndef GL_MOCK_H
#define GL_MOCK_H

#include <GL/glew.h>
#include <map>
#include <vector>
#include <array>
#include <memory>
#include <string>
#include "test_mock.h"

/**
 * @brief GLMock provides mock implementations for OpenGL functions
 * 
 * This class allows for intercepting and mocking OpenGL function calls during testing.
 * It provides default implementations for common GL functions and allows for
 * custom mock implementations to be registered.
 */
class GLMock {
public:
    /**
     * @brief Initialize the GL mock system
     * 
     * This should be called at the beginning of tests that use GL mocking
     */
    static void setup() {
        resetState();
    }

    /**
     * @brief Resets all mock state to defaults
     * 
     * This should be called between test cases to ensure a clean slate
     */
    static void resetState() {
        s_textureIds.clear();
        s_nextTextureId = 1; // OpenGL never uses 0 as a valid texture ID
        s_framebufferIds.clear();
        s_nextFramebufferId = 1;
        s_uniformLocations.clear();
        s_textures.clear();
        s_glErrors.clear();
        s_activeTexture = GL_TEXTURE0;
        s_boundTexture = 0;
        s_boundFramebuffer = 0;
        s_boundPBO = 0;
        s_programId = 1;
        s_lastError = GL_NO_ERROR;
    }

    /**
     * @brief Sets an error to be returned by glGetError
     * 
     * @param error The OpenGL error code to be returned
     */
    static void setError(GLenum error) {
        s_glErrors.push_back(error);
    }

    /**
     * @brief Simulates glGetError behavior
     * 
     * @return The next error in the queue, or GL_NO_ERROR if queue is empty
     */
    static GLenum getError() {
        if (s_glErrors.empty()) {
            return GL_NO_ERROR;
        } else {
            GLenum error = s_glErrors.front();
            s_glErrors.erase(s_glErrors.begin());
            return error;
        }
    }

    /**
     * @brief Mocks texture creation
     * 
     * @param n Number of textures to generate
     * @param textures Array where texture IDs will be stored
     */
    static void genTextures(GLsizei n, GLuint* textures) {
        for (GLsizei i = 0; i < n; i++) {
            textures[i] = s_nextTextureId++;
            s_textureIds.insert(textures[i]);
            
            // Initialize texture data
            s_textures[textures[i]] = std::make_unique<TextureData>();
        }
    }

    /**
     * @brief Mocks texture deletion
     * 
     * @param n Number of textures to delete
     * @param textures Array of texture IDs to delete
     */
    static void deleteTextures(GLsizei n, const GLuint* textures) {
        for (GLsizei i = 0; i < n; i++) {
            s_textureIds.erase(textures[i]);
            s_textures.erase(textures[i]);
        }
    }

    /**
     * @brief Mocks glBindTexture function
     * 
     * @param target Texture target (e.g., GL_TEXTURE_2D)
     * @param texture Texture ID to bind
     */
    static void bindTexture(GLenum target, GLuint texture) {
        if (target == GL_TEXTURE_2D) {
            s_boundTexture = texture;
        }
    }

    /**
     * @brief Mocks glActiveTexture function
     * 
     * @param texture The active texture unit
     */
    static void activeTexture(GLenum texture) {
        s_activeTexture = texture;
    }

    /**
     * @brief Mocks glTexParameteri function
     * 
     * @param target Texture target
     * @param pname Parameter name
     * @param param Parameter value
     */
    static void texParameteri(GLenum target, GLenum pname, GLint param) {
        if (s_boundTexture == 0 || s_textureIds.find(s_boundTexture) == s_textureIds.end()) {
            setError(GL_INVALID_OPERATION);
            return;
        }
        
        auto& texture = s_textures[s_boundTexture];
        texture->parameters[pname] = param;
    }

    /**
     * @brief Mocks glTexImage2D function
     * 
     * @param target Texture target
     * @param level Mipmap level
     * @param internalformat Internal format
     * @param width Texture width
     * @param height Texture height
     * @param border Border
     * @param format Pixel data format
     * @param type Pixel data type
     * @param data Pixel data
     */
    static void texImage2D(GLenum target, GLint level, GLint internalformat, 
                          GLsizei width, GLsizei height, GLint border,
                          GLenum format, GLenum type, const void* data) {
        if (s_boundTexture == 0 || s_textureIds.find(s_boundTexture) == s_textureIds.end()) {
            setError(GL_INVALID_OPERATION);
            return;
        }
        
        auto& texture = s_textures[s_boundTexture];
        texture->width = width;
        texture->height = height;
        texture->format = format;
        texture->internalFormat = internalformat;
        texture->type = type;
        
        // Store texture data if provided
        if (data) {
            size_t pixelSize = 4; // Default to RGBA
            if (format == GL_RED) pixelSize = 1;
            else if (format == GL_RG) pixelSize = 2;
            else if (format == GL_RGB) pixelSize = 3;
            
            size_t dataSize = width * height * pixelSize;
            
            if (type == GL_FLOAT) {
                dataSize *= sizeof(float);
            } else if (type == GL_UNSIGNED_BYTE) {
                // Size remains the same
            }
            
            texture->data.resize(dataSize);
            std::memcpy(texture->data.data(), data, dataSize);
        } else {
            // Initialize with zeros if no data provided
            size_t pixelSize = 4; // Default to RGBA
            if (format == GL_RED) pixelSize = 1;
            else if (format == GL_RG) pixelSize = 2;
            else if (format == GL_RGB) pixelSize = 3;
            
            size_t dataSize = width * height * pixelSize;
            
            if (type == GL_FLOAT) {
                dataSize *= sizeof(float);
            }
            
            texture->data.resize(dataSize, 0);
        }
    }

    /**
     * @brief Mocks glTexSubImage2D function
     * 
     * @param target Texture target
     * @param level Mipmap level
     * @param xoffset X offset
     * @param yoffset Y offset
     * @param width Width
     * @param height Height
     * @param format Pixel data format
     * @param type Pixel data type
     * @param data Pixel data
     */
    static void texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, 
                             GLsizei width, GLsizei height, GLenum format, 
                             GLenum type, const void* data) {
        if (s_boundTexture == 0 || s_textureIds.find(s_boundTexture) == s_textureIds.end()) {
            setError(GL_INVALID_OPERATION);
            return;
        }
        
        auto& texture = s_textures[s_boundTexture];
        
        // Simple implementation - just replace the whole texture data
        if (data) {
            size_t pixelSize = 4; // Default to RGBA
            if (format == GL_RED) pixelSize = 1;
            else if (format == GL_RG) pixelSize = 2;
            else if (format == GL_RGB) pixelSize = 3;
            
            size_t dataSize = width * height * pixelSize;
            
            if (type == GL_FLOAT) {
                dataSize *= sizeof(float);
            } else if (type == GL_UNSIGNED_BYTE) {
                // Size remains the same
            }
            
            // For simplicity, we just replace the entire texture
            // A more accurate implementation would handle partial updates with offsets
            texture->data.resize(dataSize);
            std::memcpy(texture->data.data(), data, dataSize);
        }
    }

    /**
     * @brief Mocks glGetTexImage function
     * 
     * @param target Texture target
     * @param level Mipmap level
     * @param format Pixel data format
     * @param type Pixel data type
     * @param pixels Destination for pixel data
     */
    static void getTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* pixels) {
        if (s_boundTexture == 0 || s_textureIds.find(s_boundTexture) == s_textureIds.end()) {
            setError(GL_INVALID_OPERATION);
            return;
        }
        
        auto& texture = s_textures[s_boundTexture];
        
        if (pixels && !texture->data.empty()) {
            std::memcpy(pixels, texture->data.data(), texture->data.size());
        }
    }

    /**
     * @brief Mocks framebuffer creation
     * 
     * @param n Number of framebuffers to generate
     * @param framebuffers Array where framebuffer IDs will be stored
     */
    static void genFramebuffers(GLsizei n, GLuint* framebuffers) {
        for (GLsizei i = 0; i < n; i++) {
            framebuffers[i] = s_nextFramebufferId++;
            s_framebufferIds.insert(framebuffers[i]);
        }
    }

    /**
     * @brief Mocks framebuffer deletion
     * 
     * @param n Number of framebuffers to delete
     * @param framebuffers Array of framebuffer IDs to delete
     */
    static void deleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
        for (GLsizei i = 0; i < n; i++) {
            s_framebufferIds.erase(framebuffers[i]);
        }
    }

    /**
     * @brief Mocks glBindFramebuffer function
     * 
     * @param target Framebuffer target
     * @param framebuffer Framebuffer ID to bind
     */
    static void bindFramebuffer(GLenum target, GLuint framebuffer) {
        s_boundFramebuffer = framebuffer;
    }

    /**
     * @brief Mocks glFramebufferTexture2D function
     * 
     * @param target Framebuffer target
     * @param attachment Attachment point
     * @param textarget Texture target
     * @param texture Texture ID
     * @param level Mipmap level
     */
    static void framebufferTexture2D(GLenum target, GLenum attachment, 
                                    GLenum textarget, GLuint texture, GLint level) {
        // Just record the attachment for verification
        if (s_boundFramebuffer != 0) {
            // No need to implement the full functionality for basic tests
        }
    }

    /**
     * @brief Mocks glGetUniformLocation function
     * 
     * @param program Program ID
     * @param name Uniform name
     * @return Location of the uniform
     */
    static GLint getUniformLocation(GLuint program, const char* name) {
        // For testing, return a predictable value based on the name
        std::string uniformName(name);
        
        if (s_uniformLocations.find(uniformName) == s_uniformLocations.end()) {
            // Generate a location for this uniform
            s_uniformLocations[uniformName] = static_cast<GLint>(s_uniformLocations.size() + 1);
        }
        
        return s_uniformLocations[uniformName];
    }

    /**
     * @brief Mocks glUniform1i function
     * 
     * @param location Uniform location
     * @param v0 Value
     */
    static void uniform1i(GLint location, GLint v0) {
        // No need to implement for basic tests
    }

    /**
     * @brief Mocks glDrawBuffers function
     * 
     * @param n Number of buffers
     * @param bufs Array of buffer enums
     */
    static void drawBuffers(GLsizei n, const GLenum* bufs) {
        // No need to implement for basic tests
    }

    /**
     * @brief Mocks glClear function
     * 
     * @param mask Bit mask of buffers to clear
     */
    static void clear(GLbitfield mask) {
        // No need to implement for basic tests
    }

    /**
     * @brief Gets the currently bound texture
     * 
     * @return The texture ID currently bound
     */
    static GLuint getBoundTexture() {
        return s_boundTexture;
    }

    /**
     * @brief Gets the currently active texture unit
     * 
     * @return The active texture unit
     */
    static GLenum getActiveTexture() {
        return s_activeTexture;
    }

    /**
     * @brief Gets the currently bound framebuffer
     * 
     * @return The framebuffer ID currently bound
     */
    static GLuint getBoundFramebuffer() {
        return s_boundFramebuffer;
    }

private:
    // Structure to hold texture data
    struct TextureData {
        GLsizei width = 0;
        GLsizei height = 0;
        GLenum format = GL_RGBA;
        GLenum internalFormat = GL_RGBA;
        GLenum type = GL_UNSIGNED_BYTE;
        std::vector<uint8_t> data;
        std::map<GLenum, GLint> parameters;
    };

    // Static mock state
    static std::set<GLuint> s_textureIds;
    static std::set<GLuint> s_framebufferIds;
    static std::map<std::string, GLint> s_uniformLocations;
    static std::map<GLuint, std::unique_ptr<TextureData>> s_textures;
    static std::vector<GLenum> s_glErrors;

    static GLuint s_nextTextureId;
    static GLuint s_nextFramebufferId;
    static GLenum s_activeTexture;
    static GLuint s_boundTexture;
    static GLuint s_boundFramebuffer;
    static GLuint s_boundPBO;
    static GLuint s_programId;
    static GLenum s_lastError;
};

// Initialize static variables
std::set<GLuint> GLMock::s_textureIds;
std::set<GLuint> GLMock::s_framebufferIds;
std::map<std::string, GLint> GLMock::s_uniformLocations;
std::map<GLuint, std::unique_ptr<GLMock::TextureData>> GLMock::s_textures;
std::vector<GLenum> GLMock::s_glErrors;

GLuint GLMock::s_nextTextureId = 1;
GLuint GLMock::s_nextFramebufferId = 1;
GLenum GLMock::s_activeTexture = GL_TEXTURE0;
GLuint GLMock::s_boundTexture = 0;
GLuint GLMock::s_boundFramebuffer = 0;
GLuint GLMock::s_boundPBO = 0;
GLuint GLMock::s_programId = 1;
GLenum GLMock::s_lastError = GL_NO_ERROR;

/**
 * @brief Macro for setting up GL mock functions in test cases
 * 
 * This macro sets up the mock functions for the duration of the current test case scope.
 */
#define GL_MOCK_SETUP() \
    GLMock::setup(); \
    Mock::when("glGenTextures", GLMock::genTextures); \
    Mock::when("glDeleteTextures", GLMock::deleteTextures); \
    Mock::when("glBindTexture", GLMock::bindTexture); \
    Mock::when("glActiveTexture", GLMock::activeTexture); \
    Mock::when("glTexParameteri", GLMock::texParameteri); \
    Mock::when("glTexImage2D", GLMock::texImage2D); \
    Mock::when("glTexSubImage2D", GLMock::texSubImage2D); \
    Mock::when("glGetTexImage", GLMock::getTexImage); \
    Mock::when("glGenFramebuffers", GLMock::genFramebuffers); \
    Mock::when("glDeleteFramebuffers", GLMock::deleteFramebuffers); \
    Mock::when("glBindFramebuffer", GLMock::bindFramebuffer); \
    Mock::when("glFramebufferTexture2D", GLMock::framebufferTexture2D); \
    Mock::when("glGetUniformLocation", GLMock::getUniformLocation); \
    Mock::when("glUniform1i", GLMock::uniform1i); \
    Mock::when("glDrawBuffers", GLMock::drawBuffers); \
    Mock::when("glClear", GLMock::clear); \
    Mock::when("glGetError", GLMock::getError);

#endif // GL_MOCK_H