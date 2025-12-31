#ifndef TRACK_DISPLAY_COMPONENT_H
#define TRACK_DISPLAY_COMPONENT_H

#include <vector>
#include <memory>
#include "graphics_core/graphics_component.h"
#include "utilities/shader_program.h"

// Component that renders all the ticks (measure/ruler) directly
class TrackMeasureComponent : public GraphicsComponent {
public:
    TrackMeasureComponent(
        float x, float y, float width, float height,
        PositionMode position_mode = PositionMode::TOP_LEFT,
        size_t num_ticks = 10
    );
    ~TrackMeasureComponent() override;

protected:
    bool initialize() override;
    void render_content() override;

private:
    size_t m_num_ticks;
    std::unique_ptr<AudioShaderProgram> m_shader_program;
    GLuint m_vao, m_vbo;
    size_t m_vertex_count;  // Always 2 (one line segment)
};

// Component for rendering a single track row (horizontal bar)
class TrackRowComponent : public GraphicsComponent {
public:
    TrackRowComponent(float x, float y, float width, float height,
                     PositionMode position_mode = PositionMode::TOP_LEFT);
    ~TrackRowComponent() override;

protected:
    bool initialize() override;
    void render_content() override;

private:
    std::unique_ptr<AudioShaderProgram> m_shader_program;
    GLuint m_vao, m_vbo;
};

// Main component that contains the measure and track rows
class TrackDisplayComponent : public GraphicsComponent {
public:
    TrackDisplayComponent(
        float x, float y, float width, float height,
        PositionMode position_mode = PositionMode::TOP_LEFT,
        size_t num_tracks = 6,
        size_t num_ticks = 10
    );
    ~TrackDisplayComponent() override;

    // Override render to apply full-component effects
    void render() override;

protected:
    bool initialize() override;
    void render_content() override;

private:
    size_t m_num_tracks;
    size_t m_num_ticks;
    TrackMeasureComponent* m_measure_component;
    std::vector<TrackRowComponent*> m_track_components;
    
    // FBO for post-processing effects
    GLuint m_fbo = 0;
    GLuint m_fbo_texture = 0;
    GLuint m_depth_buffer = 0; // In case we need depth, though likely not for 2D
    int m_fbo_width = 0;
    int m_fbo_height = 0;
    
    // Resources for compositing
    std::unique_ptr<AudioShaderProgram> m_composite_shader;
    GLuint m_quad_vao = 0;
    GLuint m_quad_vbo = 0;
    
    void layout_components();
    void initialize_fbo(int width, int height);
    void render_composite(int window_width, int window_height);
};

#endif // TRACK_DISPLAY_COMPONENT_H

