#include <iostream>
#include <GL/glew.h>
#include "graphics_components/text_component.h"
#include "utilities/shader_program.h"

// Initialize static members
std::unique_ptr<AudioShaderProgram> TextComponent::s_text_shader = nullptr;
GLuint TextComponent::s_text_vao = 0;
GLuint TextComponent::s_text_vbo = 0;
bool TextComponent::s_graphics_initialized = false;
bool TextComponent::s_ttf_initialized = false;
std::unordered_map<std::string, TextComponent::FontInfo> TextComponent::s_fonts;

TextComponent::TextComponent(
    float x, 
    float y, 
    float width, 
    float height, 
    const std::string& text
) : GraphicsComponent(x, y, width, height),
    m_text(text)
{
    initialize_ttf();
    initialize_default_font();
    initialize_static_graphics();
    initialize_text();
}

TextComponent::~TextComponent() {
    // Clean up OpenGL resources
    if (m_text_texture != 0) {
        glDeleteTextures(1, &m_text_texture);
    }
}

void TextComponent::initialize_ttf() {
    // Initialize SDL_ttf if it hasn't been initialized yet
    if (!s_ttf_initialized) {
        if (TTF_Init() == -1) {
            printf("SDL_ttf could not initialize! SDL_ttf Error: %s\n", TTF_GetError());
        } else {
            s_ttf_initialized = true;
        }
    }
}

void TextComponent::initialize_default_font() {
    // Only initialize default font if it doesn't already exist
    if (s_fonts.find("default") == s_fonts.end()) {
        // Check if our custom font exists
        const std::string project_font = "media/fonts/arial.ttf";
        
        // Try to load our project font
        if (load_font("default", project_font)) {
            return;
        }
        
        // Fallback to system fonts if our font doesn't exist
        const char* font_paths[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",  // Linux
            "/Library/Fonts/Arial Unicode.ttf",                 // macOS
            "C:\\Windows\\Fonts\\arial.ttf"                     // Windows
        };
        
        for (const char* path : font_paths) {
            if (load_font("default", path)) {
                return;
            }
        }
        
        printf("Failed to load any default font!\n");
    }
}

bool TextComponent::load_font(const std::string& font_name, const std::string& font_path, int default_size) {
    if (!s_ttf_initialized) {
        initialize_ttf();
    }
    
    // Try to open the font
    TTF_Font* font = TTF_OpenFont(font_path.c_str(), default_size);
    if (!font) {
        printf("Failed to load font %s! SDL_ttf Error: %s\n", font_path.c_str(), TTF_GetError());
        return false;
    }
    
    // Store the font info
    FontInfo font_info;
    font_info.path = font_path;
    font_info.sized_fonts[default_size] = font;
    
    // Add to font map
    s_fonts[font_name] = font_info;
    
    printf("Loaded font '%s' from %s\n", font_name.c_str(), font_path.c_str());
    return true;
}

TTF_Font* TextComponent::get_sized_font() {
    // Check if font exists
    auto font_it = s_fonts.find(m_font_name);
    if (font_it == s_fonts.end()) {
        // Fall back to default font
        font_it = s_fonts.find("default");
        if (font_it == s_fonts.end()) {
            return nullptr;
        }
    }
    
    // Check if we already have this size
    auto& font_info = font_it->second;
    auto size_it = font_info.sized_fonts.find(m_font_size);
    if (size_it != font_info.sized_fonts.end()) {
        return size_it->second;
    }
    
    // Create a new font with the requested size
    TTF_Font* font = TTF_OpenFont(font_info.path.c_str(), m_font_size);
    if (!font) {
        printf("Failed to create font %s at size %d! SDL_ttf Error: %s\n", 
               m_font_name.c_str(), m_font_size, TTF_GetError());
        return nullptr;
    }
    
    // Store and return the new font
    font_info.sized_fonts[m_font_size] = font;
    return font;
}

std::vector<std::string> TextComponent::get_available_fonts() {
    std::vector<std::string> font_names;
    font_names.reserve(s_fonts.size());
    
    for (const auto& font_pair : s_fonts) {
        font_names.push_back(font_pair.first);
    }
    
    return font_names;
}

bool TextComponent::set_font(const std::string& font_name) {
    if (s_fonts.find(font_name) != s_fonts.end()) {
        m_font_name = font_name;
        initialize_text(); // Recreate text with new font
        return true;
    }
    return false;
}

void TextComponent::initialize_static_graphics() {
    if (!s_graphics_initialized) {
        // Create a simple texture rendering program
        const std::string vertex_shader_src = R"(
            #version 330 core
            layout (location = 0) in vec2 aPos;
            layout (location = 1) in vec2 aTexCoord;
            
            out vec2 TexCoord;
            
            uniform vec2 uPosition;
            uniform vec2 uDimensions;
            
            void main() {
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
            
            void main() {
                FragColor = texture(uTexture, TexCoord);
            }
        )";
        
        s_text_shader = std::make_unique<AudioShaderProgram>(vertex_shader_src, fragment_shader_src);
        if (!s_text_shader->initialize()) {
            printf("Failed to initialize text shader program\n");
            return;
        }
        
        // Create a quad with texture coordinates for rendering text
        float vertices[] = {
            // positions    // texture coords
            -1.0f, -1.0f,   0.0f, 1.0f,  // bottom left
            -1.0f,  1.0f,   0.0f, 0.0f,  // top left
             1.0f,  1.0f,   1.0f, 0.0f,  // top right
            
            -1.0f, -1.0f,   0.0f, 1.0f,  // bottom left
             1.0f,  1.0f,   1.0f, 0.0f,  // top right
             1.0f, -1.0f,   1.0f, 1.0f   // bottom right
        };
        
        glGenVertexArrays(1, &s_text_vao);
        glGenBuffers(1, &s_text_vbo);
        
        glBindVertexArray(s_text_vao);
        glBindBuffer(GL_ARRAY_BUFFER, s_text_vbo);
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

void TextComponent::initialize_text() {
    // Delete old texture if it exists
    if (m_text_texture != 0) {
        glDeleteTextures(1, &m_text_texture);
        m_text_texture = 0;
    }
    
    if (m_text.empty()) return;
    
    TTF_Font* font = get_sized_font();
    if (!font) {
        printf("No font available for rendering text\n");
        return;
    }
    
    // Create surface from text
    SDL_Color color = {
        static_cast<Uint8>(m_text_color[0] * 255),
        static_cast<Uint8>(m_text_color[1] * 255),
        static_cast<Uint8>(m_text_color[2] * 255),
        static_cast<Uint8>(m_text_color[3] * 255)
    };
    
    SDL_Surface* surface = TTF_RenderText_Blended(font, m_text.c_str(), color);
    
    if (!surface) {
        printf("Unable to render text surface! SDL_ttf Error: %s\n", TTF_GetError());
        return;
    }
    
    // Create texture from surface
    glGenTextures(1, &m_text_texture);
    glBindTexture(GL_TEXTURE_2D, m_text_texture);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Upload texture data
    glTexImage2D(
        GL_TEXTURE_2D, 
        0, 
        GL_RGBA, 
        surface->w, 
        surface->h, 
        0, 
        GL_RGBA, 
        GL_UNSIGNED_BYTE, 
        surface->pixels
    );
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Free surface
    SDL_FreeSurface(surface);
}

void TextComponent::render() {
    if (m_text_texture == 0 || m_text.empty()) return;
    
    // Bind texture
    glBindTexture(GL_TEXTURE_2D, m_text_texture);
    
    // Save current state
    GLint current_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);
    
    glUseProgram(s_text_shader->get_program());
    
    // Set uniforms
    glUniform2f(glGetUniformLocation(s_text_shader->get_program(), "uPosition"), m_x, m_y);
    glUniform2f(glGetUniformLocation(s_text_shader->get_program(), "uDimensions"), m_width, m_height);
    glUniform1i(glGetUniformLocation(s_text_shader->get_program(), "uTexture"), 0);
    
    // Enable blending for text
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Draw text
    glBindVertexArray(s_text_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    // Restore state
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(current_program);
    glDisable(GL_BLEND);
    
    // Render children
    GraphicsComponent::render();
}

void TextComponent::set_text(const std::string& text) {
    if (m_text == text) return;
    m_text = text;
    initialize_text(); // Recreate text texture with new text
}

void TextComponent::set_text_color(float r, float g, float b, float a) {
    m_text_color[0] = r;
    m_text_color[1] = g;
    m_text_color[2] = b;
    m_text_color[3] = a;
    initialize_text(); // Recreate text texture with new color
}

void TextComponent::set_font_size(int size) {
    if (m_font_size == size) return;
    m_font_size = size;
    initialize_text(); // Recreate text texture with new font size
}

void TextComponent::set_horizontal_alignment(float alignment) {
    m_horizontal_alignment = alignment;
    // No need to recreate texture, just affects positioning
}

void TextComponent::set_vertical_alignment(float alignment) {
    m_vertical_alignment = alignment;
    // No need to recreate texture, just affects positioning
}