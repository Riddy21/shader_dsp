#include <iostream>
#include "graphics_components/image_component.h"
#include "utilities/shader_program.h"

// Initialize static members
std::unique_ptr<AudioShaderProgram> ImageComponent::s_image_shader = nullptr;
GLuint ImageComponent::s_image_vao = 0;
GLuint ImageComponent::s_image_vbo = 0;
bool ImageComponent::s_graphics_initialized = false;
bool ImageComponent::s_img_initialized = false;

ImageComponent::ImageComponent(
    PositionMode position_mode,
    float x, 
    float y, 
    float width, 
    float height, 
    const std::string& image_path
) : GraphicsComponent(position_mode, x, y, width, height),
    m_image_path(image_path)
{
    // Initialize scaling parameters with defaults
    m_scaling_params.scale_mode = ContentScaling::ScaleMode::FIT;
    m_scaling_params.horizontal_alignment = 0.5;
    m_scaling_params.vertical_alignment = 0.5;
    m_scaling_params.custom_aspect_ratio = 1.0f; // Use natural aspect ratio
    
}

ImageComponent::ImageComponent(
    float x, 
    float y, 
    float width, 
    float height, 
    const std::string& image_path
) : ImageComponent(PositionMode::CORNER, x, y, width, height, image_path)
{
}

ImageComponent::~ImageComponent() {
    // Clean up OpenGL resources
    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
    }
}

bool ImageComponent::initialize() {
    // Initialize SDL_image (non-OpenGL initialization)
    initialize_img();

    // Initialize static graphics resources
    initialize_static_graphics();
    
    // Load the image if a path was provided
    if (!m_image_path.empty()) {
        if (!load_image(m_image_path)) {
            std::cerr << "Failed to load image: " << m_image_path << std::endl;
            return false;
        }
    }
    
    GraphicsComponent::initialize();
    return true;
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
            #version 300 es
            layout (location = 0) in vec2 aPos;
            layout (location = 1) in vec2 aTexCoord;
            
            uniform float uRotation;
            uniform float uAspectRatio;
            
            out vec2 TexCoord;
            
            void main() {
                // Apply aspect ratio correction for viewport
                // The viewport is non-square, so we need to correct for that by transforming 
                // to a square space (physically uniform), rotating, then transforming back.
                
                // 1. Scale X by aspect ratio to match Y scale (physically square space)
                vec2 square_pos = vec2(aPos.x * uAspectRatio, aPos.y);
                
                // 2. Apply rotation
                float cos_angle = cos(uRotation);
                float sin_angle = sin(uRotation);
                mat2 rotation_matrix = mat2(
                    cos_angle, -sin_angle,
                    sin_angle,  cos_angle
                );
                vec2 rotated_pos = rotation_matrix * square_pos;
                
                // 3. Scale X back (undo step 1)
                vec2 final_pos = vec2(rotated_pos.x / uAspectRatio, rotated_pos.y);
                
                // Using our coordinate system: 1 is top, -1 is bottom
                gl_Position = vec4(final_pos, 0.0, 1.0);
                TexCoord = aTexCoord;
            }
        )";

        const std::string fragment_shader_src = R"(
            #version 300 es
            precision mediump float;
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
    m_natural_aspect_ratio = static_cast<float>(surface->w) / static_cast<float>(surface->h);
    
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
    if (m_texture == 0 || !s_graphics_initialized || !s_image_shader) return;
    
    // Bind texture
    glBindTexture(GL_TEXTURE_2D, m_texture);
    
    // Save current state
    GLint current_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);
    
    glUseProgram(s_image_shader->get_program());
    
    // Calculate the component aspect ratio
    float component_aspect = m_width / m_height;
    
    // Get display aspect ratio from render context
    float display_aspect = m_render_context.get_aspect_ratio();
    
    // The true viewport aspect ratio combines component dimensions and the window aspect ratio
    float viewport_aspect = component_aspect * display_aspect;
    
    // Set uniforms
    glUniform1i(glGetUniformLocation(s_image_shader->get_program(), "uTexture"), 0);
    glUniform4f(glGetUniformLocation(s_image_shader->get_program(), "uTintColor"),
                m_tint_color[0], m_tint_color[1], m_tint_color[2], m_tint_color[3]);
    glUniform1f(glGetUniformLocation(s_image_shader->get_program(), "uRotation"), m_rotation);
    glUniform1f(glGetUniformLocation(s_image_shader->get_program(), "uAspectRatio"), viewport_aspect);
    
    // Calculate the vertex data using the content scaling utility
    auto vertex_data = ContentScaling::calculateVertexData(
        m_natural_aspect_ratio,
        component_aspect, 
        display_aspect,
        m_scaling_params
    );
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Update the vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, s_image_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * vertex_data.size(), vertex_data.data());
    
    // Draw image
    glBindVertexArray(s_image_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    // Restore state
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
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

void ImageComponent::set_rotation(float angle_radians) {
    m_rotation = angle_radians;
}

// New ContentScaling API
void ImageComponent::set_scale_mode(ContentScaling::ScaleMode mode) {
    m_scaling_params.scale_mode = mode;
}

void ImageComponent::set_horizontal_alignment(const float alignment) {
    m_scaling_params.horizontal_alignment = alignment;
}

void ImageComponent::set_vertical_alignment(const float alignment) {
    m_scaling_params.vertical_alignment = alignment;
}

void ImageComponent::set_aspect_ratio(const float ratio) {
    m_scaling_params.custom_aspect_ratio = ratio;
}

// Legacy API (for backward compatibility)
void ImageComponent::set_scale_mode(ScaleMode mode) {
    switch (mode) {
        case ScaleMode::STRETCH:
            m_scaling_params.scale_mode = ContentScaling::ScaleMode::STRETCH;
            break;
        case ScaleMode::CONTAIN:
            m_scaling_params.scale_mode = ContentScaling::ScaleMode::FIT;
            break;
        case ScaleMode::COVER:
            m_scaling_params.scale_mode = ContentScaling::ScaleMode::FILL;
            break;
    }
}