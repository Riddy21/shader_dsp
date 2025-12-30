#ifndef SPRITE_COMPONENT_H
#define SPRITE_COMPONENT_H

#include <memory>
#include <string>
#include <vector>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include "utilities/shader_program.h"
#include "graphics_core/graphics_component.h"
#include "graphics_core/content_scaling.h"

class SpriteComponent : public GraphicsComponent {
public:
    SpriteComponent(float x, float y, float width, float height, 
                   const std::vector<std::string>& frame_paths,
                   PositionMode position_mode = PositionMode::TOP_LEFT);
    ~SpriteComponent() override;

    void render_content() override;

    // Load frames from a vector of file paths
    bool load_frames(const std::vector<std::string>& frame_paths);
    
    // Set animation frame rate (frames per second)
    void set_frame_rate(float fps);
    
    // Get current frame rate
    float get_frame_rate() const { return m_frame_rate; }
    
    // Set whether animation loops (default: true)
    void set_looping(bool looping);
    
    // Get whether animation loops
    bool get_looping() const { return m_looping; }
    
    // Start/stop animation
    void play();
    void pause();
    void stop();
    
    // Set current frame manually (0-based index)
    void set_frame(int frame_index);
    
    // Get current frame index
    int get_current_frame() const { return m_current_frame; }
    
    // Get total number of frames
    size_t get_frame_count() const { return m_textures.size(); }
    
    // Set tint color (applied as a multiplication to the sprite)
    void set_tint_color(float r, float g, float b, float a);
    
    // Set rotation angle in radians
    void set_rotation(float angle_radians);
    
    // Content scaling methods (using the unified ContentScaling API)
    void set_scale_mode(ContentScaling::ScaleMode mode);
    void set_horizontal_alignment(const float alignment);
    void set_vertical_alignment(const float alignment);
    void set_aspect_ratio(const float ratio);
    
    // Set maximum resolution for texture loading (0 = no limit)
    // Images larger than this will be scaled down while maintaining aspect ratio
    void set_max_resolution(int max_width, int max_height);
    
    // Get maximum resolution
    int get_max_width() const { return m_max_width; }
    int get_max_height() const { return m_max_height; }

protected:
    bool initialize() override;

private:
    void create_texture_from_surface(SDL_Surface* surface, size_t frame_index);
    void update_animation();
    
    std::vector<std::string> m_frame_paths;
    std::vector<GLuint> m_textures;
    float m_tint_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float m_natural_aspect_ratio = 1.0f;
    float m_rotation = 0.0f; // Rotation angle in radians
    ContentScaling::ScalingParams m_scaling_params;
    int m_max_width = 0;  // 0 means no limit
    int m_max_height = 0; // 0 means no limit
    
    // Animation state
    float m_frame_rate = 10.0f; // frames per second
    bool m_looping = true;
    bool m_playing = true;
    int m_current_frame = 0;
    Uint32 m_last_frame_time = 0;
    
    static std::unique_ptr<AudioShaderProgram> s_sprite_shader;
    static GLuint s_sprite_vao;
    static GLuint s_sprite_vbo;
    static bool s_graphics_initialized;
    static bool s_img_initialized;
    
    static void initialize_img();
    static void initialize_static_graphics();
};

#endif // SPRITE_COMPONENT_H

