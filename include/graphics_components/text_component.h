#ifndef TEXT_COMPONENT_H
#define TEXT_COMPONENT_H

#include <memory>
#include <string>
#include <unordered_map>
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "utilities/shader_program.h"
#include "graphics_core/graphics_component.h"

class TextComponent : public GraphicsComponent {
public:
    TextComponent(float x, float y, float width, float height, const std::string& text);
    ~TextComponent() override;

    void render() override;

    void set_text(const std::string& text);
    const std::string& get_text() const { return m_text; }
    
    void set_text_color(float r, float g, float b, float a);
    void set_font_size(int size);
    void set_horizontal_alignment(float alignment); // 0.0 = left, 0.5 = center, 1.0 = right
    void set_vertical_alignment(float alignment);   // 0.0 = top, 0.5 = center, 1.0 = bottom
    
    // Set font by name (must be loaded first)
    bool set_font(const std::string& font_name);
    
    // Add a new font to the available fonts
    static bool load_font(const std::string& font_name, const std::string& font_path, int default_size = 16);
    
    // Get list of available fonts
    static std::vector<std::string> get_available_fonts();

private:
    void initialize_graphics();
    void initialize_text();
    
    // Get the appropriate font with current size
    TTF_Font* get_sized_font();

    std::string m_text;
    std::string m_font_name = "default";
    float m_text_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float m_horizontal_alignment = 0.5f; // Default to center
    float m_vertical_alignment = 0.5f;   // Default to center
    int m_font_size = 16;
    
    GLuint m_text_texture = 0;
    static std::unique_ptr<AudioShaderProgram> s_text_shader;
    static GLuint s_text_vao;
    static GLuint s_text_vbo;
    static bool s_graphics_initialized;
    
    // Font management
    struct FontInfo {
        std::string path;
        std::unordered_map<int, TTF_Font*> sized_fonts; // Size -> Font mapping
    };
    
    static std::unordered_map<std::string, FontInfo> s_fonts;
    static bool s_ttf_initialized;
    static void initialize_ttf();
    static void initialize_static_graphics();
    
    // Initialize default font
    static void initialize_default_font();
};

#endif // TEXT_COMPONENT_H