#include <iostream>
#include <GL/glew.h>
#include "graphics_components/image_component.h"
#include "utilities/shader_program.h"

// Initialize static members
std::unique_ptr<AudioShaderProgram> ImageComponent::s_image_shader = nullptr;
GLuint ImageComponent::s_image_vao = 0;
GLuint ImageComponent::s_image_vbo = 0;
bool ImageComponent::s_graphics_initialized = false;
bool ImageComponent::s_img_initialized = false;

ImageComponent::ImageComponent(
    float x, 
    float y, 
    float width, 
    float height, 
    const std::string& image_path
) : GraphicsComponent(x, y, width, height),
    m_image_path(image_path)
{
    initialize_img();
    initialize_static_graphics();
    load_image(image_path);
}

ImageComponent::~ImageComponent() {
    // Clean up OpenGL resources
    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
    }
}

void ImageComponent::initialize_img() {
    // Initialize SDL_image if it hasn't been initialized yet
    if (!s_img_initialized) {
        int img_flags = IMG_INIT_PNG | IMG_INIT_JPG;
        if (!(IMG_Init(img_flags) & img_flags)) {
            printf("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
        } else {
            s_img_initialized = true;
        }
    }
}

void ImageComponent::initialize_static_graphics() {
    if (!s_graphics_initialized) {
        // Create a shader program for rendering images
        const std::string vertex_shader_src = R"(
            #version 330 core
            layout (location = 0) in vec2 aPos;
            layout (location = 1) in vec2 aTexCoord;
            
            out vec2 TexCoord;
            
            uniform vec2 uPosition;
            uniform vec2 uDimensions;
            
            void main() {
                // Using our coordinate system: 1 is top, -1 is bottom
                vec2 pos = aPos * uDimensions + uPosition;
                gl_Position = vec4(pos, 0.0, 1.0);
                TexCoord = aTexCoord;
            }
        )";

        const std::string fragment_shader_src = R"(
            #version 330 core
            in vec2 TexCoord;
            out vec4 FragColor;
            
            uniform sampler2D uTexture;
            uniform vec4 uTintColor;
            
            void main() {
                vec4 texColor = texture(uTexture, TexCoord);
                FragColor = texColor * uTintColor;
            }
        )";
        
        s_image_shader = std::make_unique<AudioShaderProgram>(vertex_shader_src, fragment_shader_src);
        if (!s_image_shader->initialize()) {
            printf("Failed to initialize image shader program\n");
            return;
        }
        
        // Create a quad with texture coordinates for rendering images
        float vertices[] = {
            // positions    // texture coords
            -1.0f, -1.0f,   0.0f, 1.0f,  // bottom left
            -1.0f,  1.0f,   0.0f, 0.0f,  // top left
             1.0f,  1.0f,   1.0f, 0.0f,  // top right
            
            -1.0f, -1.0f,   0.0f, 1.0f,  // bottom left
             1.0f,  1.0f,   1.0f, 0.0f,  // top right
             1.0f, -1.0f,   1.0f, 1.0f   // bottom right
        };
        
        glGenVertexArrays(1, &s_image_vao);
        glGenBuffers(1, &s_image_vbo);
        
        glBindVertexArray(s_image_vao);
        glBindBuffer(GL_ARRAY_BUFFER, s_image_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        // Position attribute
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        // Texture coordinate attribute
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        s_graphics_initialized = true;
    }
}

bool ImageComponent::load_image(const std::string& image_path) {
    if (!s_img_initialized) {
        printf("SDL_image is not initialized\n");
        return false;
    }
    
    m_image_path = image_path;
    
    // Load image
    SDL_Surface* surface = IMG_Load(image_path.c_str());
    if (!surface) {
        printf("Unable to load image %s! SDL_image Error: %s\n", image_path.c_str(), IMG_GetError());
        return false;
    }
    
    create_texture_from_surface(surface);
    
    // Free the surface
    SDL_FreeSurface(surface);
    
    return true;
}

bool ImageComponent::load_from_surface(SDL_Surface* surface) {
    if (!surface) {
        printf("Invalid surface provided\n");
        return false;
    }
    
    create_texture_from_surface(surface);
    return true;
}

void ImageComponent::create_texture_from_surface(SDL_Surface* surface) {
    // Delete old texture if it exists
    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
    
    // Calculate aspect ratio
    m_aspect_ratio = static_cast<float>(surface->w) / static_cast<float>(surface->h);
    
    // Create texture from surface
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Determine format based on SDL surface
    GLenum format;
    if (surface->format->BytesPerPixel == 4) {
        format = GL_RGBA;
    } else if (surface->format->BytesPerPixel == 3) {
        format = GL_RGB;
    } else {
        printf("Unknown image format, using RGBA\n");
        format = GL_RGBA;
    }
    
    // Upload texture data
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        format,
        surface->w,
        surface->h,
        0,
        format,
        GL_UNSIGNED_BYTE,
        surface->pixels
    );
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

void ImageComponent::render_content() {
    if (m_texture == 0) return;
    
    // Bind texture
    glBindTexture(GL_TEXTURE_2D, m_texture);
    
    // Save current state
    GLint current_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);
    
    glUseProgram(s_image_shader->get_program());
    
    // Get screen dimensions to account for screen aspect ratio
    int screen_width = 1, screen_height = 1;
    SDL_Window* window = SDL_GL_GetCurrentWindow();
    if (window) {
        SDL_Surface* surface = SDL_GetWindowSurface(window);
        if (surface) {
            screen_width = surface->w;
            screen_height = surface->h;
        }
    }
    float screen_aspect = static_cast<float>(screen_width) / screen_height;
    
    // Calculate dimensions based on scale mode
    float width = m_width;
    float height = m_height;
    
    // Adjust dimensions based on scale mode
    if (m_scale_mode != ScaleMode::STRETCH) {
        // Calculate the true component aspect ratio considering the screen aspect ratio
        float component_aspect = (m_width / m_height) * screen_aspect;
        
        // Normalize the image aspect ratio to account for screen aspect ratio
        float normalized_image_aspect = m_aspect_ratio / screen_aspect;
        
        if ((m_scale_mode == ScaleMode::CONTAIN && normalized_image_aspect > component_aspect) ||
            (m_scale_mode == ScaleMode::COVER && normalized_image_aspect < component_aspect)) {
            // Width constrained
            width = m_width;
            height = width / normalized_image_aspect;
        } else {
            // Height constrained
            height = m_height;
            width = height * normalized_image_aspect;
        }
    }
    
    // Set uniforms
    glUniform2f(glGetUniformLocation(s_image_shader->get_program(), "uPosition"), m_x, m_y);
    glUniform2f(glGetUniformLocation(s_image_shader->get_program(), "uDimensions"), width, height);
    glUniform1i(glGetUniformLocation(s_image_shader->get_program(), "uTexture"), 0);
    glUniform4f(glGetUniformLocation(s_image_shader->get_program(), "uTintColor"),
                m_tint_color[0], m_tint_color[1], m_tint_color[2], m_tint_color[3]);
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Draw image
    glBindVertexArray(s_image_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    // Restore state
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(current_program);
    glDisable(GL_BLEND);
}

void ImageComponent::set_tint_color(float r, float g, float b, float a) {
    m_tint_color[0] = r;
    m_tint_color[1] = g;
    m_tint_color[2] = b;
    m_tint_color[3] = a;
}

void ImageComponent::set_scale_mode(ScaleMode mode) {
    m_scale_mode = mode;
}