#include <iostream>
#include <algorithm>
#include "graphics_components/sprite_component.h"
#include "utilities/shader_program.h"

// Initialize static members
std::unique_ptr<AudioShaderProgram> SpriteComponent::s_sprite_shader = nullptr;
GLuint SpriteComponent::s_sprite_vao = 0;
GLuint SpriteComponent::s_sprite_vbo = 0;
bool SpriteComponent::s_graphics_initialized = false;
bool SpriteComponent::s_img_initialized = false;

SpriteComponent::SpriteComponent(
    float x, 
    float y, 
    float width, 
    float height, 
    const std::vector<std::string>& frame_paths,
    PositionMode position_mode
) : GraphicsComponent(x, y, width, height, position_mode),
    m_frame_paths(frame_paths)
{
    // Initialize scaling parameters with defaults
    m_scaling_params.scale_mode = ContentScaling::ScaleMode::FIT;
    m_scaling_params.horizontal_alignment = 0.5;
    m_scaling_params.vertical_alignment = 0.5;
    m_scaling_params.custom_aspect_ratio = 1.0f; // Use natural aspect ratio
    
    // Set default max resolution to 800x800
    m_max_width = 800;
    m_max_height = 800;
}

SpriteComponent::~SpriteComponent() {
    // Clean up OpenGL resources
    for (GLuint texture : m_textures) {
        if (texture != 0) {
            glDeleteTextures(1, &texture);
        }
    }
    m_textures.clear();
}

bool SpriteComponent::initialize() {
    // Initialize SDL_image (non-OpenGL initialization)
    initialize_img();

    // Initialize static graphics resources
    initialize_static_graphics();
    
    // Load the frames if paths were provided
    if (!m_frame_paths.empty()) {
        if (!load_frames(m_frame_paths)) {
            std::cerr << "Failed to load sprite frames" << std::endl;
            return false;
        }
    }
    
    // Initialize animation timing
    m_last_frame_time = SDL_GetTicks();
    
    GraphicsComponent::initialize();
    return true;
}

void SpriteComponent::initialize_img() {
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

void SpriteComponent::initialize_static_graphics() {
    if (!s_graphics_initialized) {
        // Create a shader program for rendering sprites (same as ImageComponent)
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
        
        s_sprite_shader = std::make_unique<AudioShaderProgram>(vertex_shader_src, fragment_shader_src);
        if (!s_sprite_shader->initialize()) {
            printf("Failed to initialize sprite shader program\n");
            return;
        }
        
        // Create a quad with texture coordinates for rendering sprites
        float vertices[] = {
            // positions    // texture coords
            -1.0f, -1.0f,   0.0f, 1.0f,  // bottom left
            -1.0f,  1.0f,   0.0f, 0.0f,  // top left
             1.0f,  1.0f,   1.0f, 0.0f,  // top right
            
            -1.0f, -1.0f,   0.0f, 1.0f,  // bottom left
             1.0f,  1.0f,   1.0f, 0.0f,  // top right
             1.0f, -1.0f,   1.0f, 1.0f   // bottom right
        };
        
        glGenVertexArrays(1, &s_sprite_vao);
        glGenBuffers(1, &s_sprite_vbo);
        
        glBindVertexArray(s_sprite_vao);
        glBindBuffer(GL_ARRAY_BUFFER, s_sprite_vbo);
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

bool SpriteComponent::load_frames(const std::vector<std::string>& frame_paths) {
    if (!s_img_initialized) {
        printf("SDL_image is not initialized\n");
        return false;
    }
    
    // Clear existing textures
    for (GLuint texture : m_textures) {
        if (texture != 0) {
            glDeleteTextures(1, &texture);
        }
    }
    m_textures.clear();
    m_textures.resize(frame_paths.size(), 0);
    
    m_frame_paths = frame_paths;
    
    // Load each frame
    for (size_t i = 0; i < frame_paths.size(); ++i) {
        SDL_Surface* surface = IMG_Load(frame_paths[i].c_str());
        if (!surface) {
            printf("Unable to load sprite frame %zu: %s! SDL_image Error: %s\n", 
                   i, frame_paths[i].c_str(), IMG_GetError());
            // Continue loading other frames even if one fails
            continue;
        }
        
        create_texture_from_surface(surface, i);
        
        // Free the surface
        SDL_FreeSurface(surface);
    }
    
    // Check if at least one frame was loaded
    bool any_loaded = false;
    for (GLuint texture : m_textures) {
        if (texture != 0) {
            any_loaded = true;
            break;
        }
    }
    
    if (!any_loaded) {
        printf("Failed to load any sprite frames\n");
        return false;
    }
    
    // Reset to first frame
    m_current_frame = 0;
    m_last_frame_time = SDL_GetTicks();
    
    return true;
}

void SpriteComponent::create_texture_from_surface(SDL_Surface* surface, size_t frame_index) {
    if (frame_index >= m_textures.size()) {
        printf("Frame index out of bounds\n");
        return;
    }
    
    // Delete old texture if it exists
    if (m_textures[frame_index] != 0) {
        glDeleteTextures(1, &m_textures[frame_index]);
        m_textures[frame_index] = 0;
    }
    
    SDL_Surface* final_surface = surface;
    SDL_Surface* scaled_surface = nullptr;
    
    // Check if we need to scale down the image
    bool needs_scaling = false;
    int target_width = surface->w;
    int target_height = surface->h;
    
    if ((m_max_width > 0 && surface->w > m_max_width) || 
        (m_max_height > 0 && surface->h > m_max_height)) {
        needs_scaling = true;
        
        // Calculate scaled dimensions maintaining aspect ratio
        if (m_max_width > 0 && m_max_height > 0) {
            // Both limits set - scale to fit within bounds
            float width_scale = static_cast<float>(m_max_width) / static_cast<float>(surface->w);
            float height_scale = static_cast<float>(m_max_height) / static_cast<float>(surface->h);
            float scale = std::min(width_scale, height_scale);
            target_width = static_cast<int>(surface->w * scale);
            target_height = static_cast<int>(surface->h * scale);
        } else if (m_max_width > 0) {
            // Only width limit
            target_width = m_max_width;
            target_height = static_cast<int>(surface->h * (static_cast<float>(m_max_width) / static_cast<float>(surface->w)));
        } else {
            // Only height limit
            target_height = m_max_height;
            target_width = static_cast<int>(surface->w * (static_cast<float>(m_max_height) / static_cast<float>(surface->h)));
        }
    }
    
    // Scale the surface if needed
    if (needs_scaling) {
        // Create a new surface with the target dimensions
        scaled_surface = SDL_CreateRGBSurfaceWithFormat(
            0,
            target_width,
            target_height,
            surface->format->BitsPerPixel,
            surface->format->format
        );
        
        if (!scaled_surface) {
            printf("Failed to create scaled surface: %s\n", SDL_GetError());
            scaled_surface = nullptr;
            final_surface = surface;
        } else {
            // Scale the original surface onto the new surface
            SDL_Rect src_rect = {0, 0, surface->w, surface->h};
            SDL_Rect dst_rect = {0, 0, target_width, target_height};
            
            if (SDL_BlitScaled(surface, &src_rect, scaled_surface, &dst_rect) != 0) {
                printf("Failed to scale surface: %s\n", SDL_GetError());
                SDL_FreeSurface(scaled_surface);
                scaled_surface = nullptr;
                final_surface = surface;
            } else {
                final_surface = scaled_surface;
            }
        }
    }
    
    // Calculate aspect ratio from original surface (before scaling)
    // Use the first frame's aspect ratio for all frames
    if (frame_index == 0) {
        m_natural_aspect_ratio = static_cast<float>(surface->w) / static_cast<float>(surface->h);
    }
    
    // Create texture from surface
    glGenTextures(1, &m_textures[frame_index]);
    glBindTexture(GL_TEXTURE_2D, m_textures[frame_index]);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Determine format based on SDL surface
    GLenum format;
    if (final_surface->format->BytesPerPixel == 4) {
        format = GL_RGBA;
    } else if (final_surface->format->BytesPerPixel == 3) {
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
        final_surface->w,
        final_surface->h,
        0,
        format,
        GL_UNSIGNED_BYTE,
        final_surface->pixels
    );
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Free the scaled surface if we created one
    if (scaled_surface != nullptr) {
        SDL_FreeSurface(scaled_surface);
    }
}

void SpriteComponent::update_animation() {
    if (!m_playing || m_textures.empty()) {
        return;
    }
    
    Uint32 current_time = SDL_GetTicks();
    Uint32 frame_duration = static_cast<Uint32>(1000.0f / m_frame_rate); // milliseconds per frame
    
    if (current_time - m_last_frame_time >= frame_duration) {
        m_current_frame++;
        
        if (m_current_frame >= static_cast<int>(m_textures.size())) {
            if (m_looping) {
                m_current_frame = 0;
            } else {
                m_current_frame = static_cast<int>(m_textures.size()) - 1;
                m_playing = false; // Stop at last frame
            }
        }
        
        m_last_frame_time = current_time;
    }
}

void SpriteComponent::render_content() {
    if (m_textures.empty() || !s_graphics_initialized || !s_sprite_shader) return;
    
    // Update animation
    update_animation();
    
    // Get the current frame texture
    if (m_current_frame < 0 || m_current_frame >= static_cast<int>(m_textures.size())) {
        return;
    }
    
    GLuint current_texture = m_textures[m_current_frame];
    if (current_texture == 0) {
        return;
    }
    
    // Bind texture
    glBindTexture(GL_TEXTURE_2D, current_texture);
    
    // Save current state
    GLint current_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);
    
    glUseProgram(s_sprite_shader->get_program());
    
    // Calculate the component aspect ratio
    float component_aspect = m_width / m_height;
    
    // Get display aspect ratio from render context
    float display_aspect = m_render_context.get_aspect_ratio();
    
    // The true viewport aspect ratio combines component dimensions and the window aspect ratio
    float viewport_aspect = component_aspect * display_aspect;
    
    // Set uniforms
    glUniform1i(glGetUniformLocation(s_sprite_shader->get_program(), "uTexture"), 0);
    glUniform4f(glGetUniformLocation(s_sprite_shader->get_program(), "uTintColor"),
                m_tint_color[0], m_tint_color[1], m_tint_color[2], m_tint_color[3]);
    glUniform1f(glGetUniformLocation(s_sprite_shader->get_program(), "uRotation"), m_rotation);
    glUniform1f(glGetUniformLocation(s_sprite_shader->get_program(), "uAspectRatio"), viewport_aspect);
    
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
    glBindBuffer(GL_ARRAY_BUFFER, s_sprite_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * vertex_data.size(), vertex_data.data());
    
    // Draw sprite
    glBindVertexArray(s_sprite_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    // Restore state
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(current_program);
    glDisable(GL_BLEND);
}

void SpriteComponent::set_frame_rate(float fps) {
    m_frame_rate = std::max(0.1f, fps); // Minimum 0.1 fps
}

void SpriteComponent::set_looping(bool looping) {
    m_looping = looping;
}

void SpriteComponent::play() {
    m_playing = true;
    m_last_frame_time = SDL_GetTicks();
}

void SpriteComponent::pause() {
    m_playing = false;
}

void SpriteComponent::stop() {
    m_playing = false;
    m_current_frame = 0;
    m_last_frame_time = SDL_GetTicks();
}

void SpriteComponent::set_frame(int frame_index) {
    if (frame_index >= 0 && frame_index < static_cast<int>(m_textures.size())) {
        m_current_frame = frame_index;
        m_last_frame_time = SDL_GetTicks();
    }
}

void SpriteComponent::set_tint_color(float r, float g, float b, float a) {
    m_tint_color[0] = r;
    m_tint_color[1] = g;
    m_tint_color[2] = b;
    m_tint_color[3] = a;
}

void SpriteComponent::set_rotation(float angle_radians) {
    m_rotation = angle_radians;
}

void SpriteComponent::set_scale_mode(ContentScaling::ScaleMode mode) {
    m_scaling_params.scale_mode = mode;
}

void SpriteComponent::set_horizontal_alignment(const float alignment) {
    m_scaling_params.horizontal_alignment = alignment;
}

void SpriteComponent::set_vertical_alignment(const float alignment) {
    m_scaling_params.vertical_alignment = alignment;
}

void SpriteComponent::set_aspect_ratio(const float ratio) {
    m_scaling_params.custom_aspect_ratio = ratio;
}

void SpriteComponent::set_max_resolution(int max_width, int max_height) {
    m_max_width = max_width;
    m_max_height = max_height;
    
    // Reload all frames with the new max resolution
    if (!m_frame_paths.empty()) {
        load_frames(m_frame_paths);
    }
}

