#ifndef IMAGE_COMPONENT_H
#define IMAGE_COMPONENT_H

#include <memory>
#include <string>
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include "utilities/shader_program.h"
#include "graphics_core/graphics_component.h"
#include "graphics_core/content_scaling.h"

class ImageComponent : public GraphicsComponent {
public:
    ImageComponent(float x, float y, float width, float height, const std::string& image_path);
    ~ImageComponent() override;

    void render_content() override;

    // Load a new image
    bool load_image(const std::string& image_path);
    
    // Load from an existing SDL_Surface
    bool load_from_surface(SDL_Surface* surface);
    
    // Set tint color (applied as a multiplication to the image)
    void set_tint_color(float r, float g, float b, float a);
    
    // Content scaling methods (using the unified ContentScaling API)
    void set_scale_mode(ContentScaling::ScaleMode mode);
    void set_horizontal_alignment(const float alignment);
    void set_vertical_alignment(const float alignment);
    void set_aspect_ratio(const float ratio);
    
    // Legacy API for backward compatibility
    enum class ScaleMode {
        STRETCH,  // Stretch to fill the component size
        CONTAIN,  // Scale to fit while maintaining aspect ratio
        COVER,    // Scale to cover the component while maintaining aspect ratio
    };
    void set_scale_mode(ScaleMode mode);
    
    // Get the aspect ratio of the loaded image
    float get_natural_aspect_ratio() const { return m_natural_aspect_ratio; }

private:
    void initialize_graphics();
    void create_texture_from_surface(SDL_Surface* surface);
    
    std::string m_image_path;
    float m_tint_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float m_natural_aspect_ratio = 1.0f;
    ContentScaling::ScalingParams m_scaling_params;
    
    GLuint m_texture = 0;
    static std::unique_ptr<AudioShaderProgram> s_image_shader;
    static GLuint s_image_vao;
    static GLuint s_image_vbo;
    static bool s_graphics_initialized;
    static bool s_img_initialized;
    
    static void initialize_img();
    static void initialize_static_graphics();
};

#endif // IMAGE_COMPONENT_H