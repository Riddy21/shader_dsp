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
#include "graphics_core/content_scaling.h"

#define DEFAULT_FONT_SIZE 64

class TextComponent : public GraphicsComponent {
public:
    TextComponent(float x, float y, float width, float height, const std::string& text);
    ~TextComponent() override;

    void render_content() override;

    void set_text(const std::string& text);
    const std::string& get_text() const { return m_text; }
    
    void set_text_color(float r, float g, float b, float a);
    void set_font_size(int size);
    
    // TODO: Consider moving this to graphics component
    // Content scaling methods (using the unified ContentScaling API)
    void set_scale_mode(const ContentScaling::ScaleMode mode);
    void set_horizontal_alignment(const float alignment);
    void set_vertical_alignment(const float alignment);
    void set_aspect_ratio(const float ratio);
    
    // Set font by name (must be loaded first)
    bool set_font(const std::string& font_name);
    
    // Add a new font to the available fonts
    static bool load_font(const std::string& font_name, const std::string& font_path, int default_size = DEFAULT_FONT_SIZE);
    
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
    int m_font_size = DEFAULT_FONT_SIZE;
    int m_texture_width = 0;
    int m_texture_height = 0;
    
    // Content scaling parameters
    ContentScaling::ScalingParams m_scaling_params;
    
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