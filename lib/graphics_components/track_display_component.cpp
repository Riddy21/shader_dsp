#include "graphics_components/track_display_component.h"
#include "utilities/shader_program.h"
#include "graphics_core/ui_color_palette.h"
#include <iostream>
#include <algorithm>

// ============================================================================
// TrackMeasureComponent Implementation
// ============================================================================

TrackMeasureComponent::TrackMeasureComponent(
    float x, float y, float width, float height,
    PositionMode position_mode,
    size_t num_ticks
) : GraphicsComponent(x, y, width, height, position_mode),
    m_num_ticks(num_ticks),
    m_shader_program(nullptr),
    m_vao(0),
    m_vbo(0),
    m_vertex_count(0)
{
    // Ensure reasonable defaults
    if (m_num_ticks == 0) m_num_ticks = 10;
}

TrackMeasureComponent::~TrackMeasureComponent() {
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
    }
}

bool TrackMeasureComponent::initialize() {
    // Shader for drawing vertical tick lines using instanced rendering
    // The shader calculates tick positions based on instance ID
    const std::string vertex_shader_src = R"(
        #version 300 es
        layout (location = 0) in vec2 aPos;  // x ignored, y used
        
        uniform float uNumTicks;  // Number of ticks
        
        void main() {
            // Calculate progress t from 0.0 to 1.0
            float t = 0.0;
            if (uNumTicks > 1.0) {
                t = float(gl_InstanceID) / (uNumTicks - 1.0);
            } else {
                t = 0.5; // Center if single tick
            }
            
            // Map t to x range [-1.0, 1.0]
            // We use a slight inset (0.995 instead of 1.0) to prevent the edge ticks
            // from being clipped by the viewport boundary or scissor test
            float x_pos = mix(-0.995, 0.995, t);
            
            // Use the y coordinate from the vertex, x calculated from instance ID
            gl_Position = vec4(x_pos, aPos.y, 0.0, 1.0);
        }
    )";

    const std::string fragment_shader_src = R"(
        #version 300 es
        precision mediump float;
        out vec4 FragColor;
        uniform vec4 uColor;
        void main() {
            FragColor = uColor;
        }
    )";

    m_shader_program = std::make_unique<AudioShaderProgram>(vertex_shader_src, fragment_shader_src);
    if (!m_shader_program->initialize()) {
        std::cerr << "Failed to initialize shader program for TrackMeasureComponent" << std::endl;
        return false;
    }

    // Create VAO and VBO for a single vertical line segment
    // We only need 2 vertices (bottom and top), and use instancing for multiple ticks
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    // Create a single vertical line segment (x ignored, calculated in shader)
    // In component-local coordinates: y=-1 is bottom, y=1 is top
    float vertices[] = {
        0.0f, -1.0f,  // x (ignored), y (bottom)
        0.0f,  1.0f   // x (ignored), y (top)
    };

    // Set up VAO
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    // Set up vertex attributes (vec2: x and y, but x will be recalculated in shader)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // We always have 2 vertices (one line segment)
    m_vertex_count = 2;

    GraphicsComponent::initialize();
    return true;
}

void TrackMeasureComponent::render_content() {
    if (!m_shader_program || m_vao == 0 || m_num_ticks == 0) return;

    glUseProgram(m_shader_program->get_program());

    // Set uniforms
    GLuint program = m_shader_program->get_program();
    glUniform4f(glGetUniformLocation(program, "uColor"), 0.7f, 0.7f, 0.7f, 0.8f);
    glUniform1f(glGetUniformLocation(program, "uNumTicks"), static_cast<float>(m_num_ticks));

    // Draw tick lines using instanced rendering
    // We have 2 vertices (one line segment) and draw m_num_ticks instances
    glBindVertexArray(m_vao);
    glLineWidth(1.0f);
    glDrawArraysInstanced(GL_LINES, 0, 2, m_num_ticks);
    glBindVertexArray(0);

    glUseProgram(0);
}

// ============================================================================
// TrackRowComponent Implementation
// ============================================================================

TrackRowComponent::TrackRowComponent(float x, float y, float width, float height,
                                     PositionMode position_mode)
    : GraphicsComponent(x, y, width, height, position_mode),
      m_shader_program(nullptr),
      m_vao(0),
      m_vbo(0)
{
}

TrackRowComponent::~TrackRowComponent() {
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
    }
}

bool TrackRowComponent::initialize() {
    // Simple shader for drawing a horizontal bar
    const std::string vertex_shader_src = R"(
        #version 300 es
        layout (location = 0) in vec2 aPos;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

    const std::string fragment_shader_src = R"(
        #version 300 es
        precision mediump float;
        out vec4 FragColor;
        uniform vec4 uColor;
        void main() {
            FragColor = uColor;
        }
    )";

    m_shader_program = std::make_unique<AudioShaderProgram>(vertex_shader_src, fragment_shader_src);
    if (!m_shader_program->initialize()) {
        std::cerr << "Failed to initialize shader program for TrackRowComponent" << std::endl;
        return false;
    }

    // Create a horizontal bar (rectangle as two triangles)
    // In component-local coordinates: (-1, -1) is bottom-left, (1, 1) is top-right
    float vertices[] = {
        -1.0f, -0.3f,  // bottom left
        -1.0f,  0.3f,  // top left
         1.0f,  0.3f,  // top right
        
        -1.0f, -0.3f,  // bottom left
         1.0f,  0.3f,  // top right
         1.0f, -0.3f   // bottom right
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    GraphicsComponent::initialize();
    return true;
}

void TrackRowComponent::render_content() {
    if (!m_shader_program || m_vao == 0) return;

    glUseProgram(m_shader_program->get_program());

    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Set track color (slightly transparent blue-gray)
    glUniform4f(glGetUniformLocation(m_shader_program->get_program(), "uColor"),
                0.4f, 0.5f, 0.6f, 0.6f);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glUseProgram(0);
}

// ============================================================================
// TrackDisplayComponent Implementation
// ============================================================================

TrackDisplayComponent::TrackDisplayComponent(
    float x, float y, float width, float height,
    PositionMode position_mode,
    size_t num_tracks,
    size_t num_ticks
) : GraphicsComponent(x, y, width, height, position_mode),
    m_num_tracks(num_tracks),
    m_num_ticks(num_ticks),
    m_measure_component(nullptr)
{
    // Ensure reasonable defaults
    if (m_num_tracks == 0) m_num_tracks = 6;
    if (m_num_ticks == 0) m_num_ticks = 10;
}

TrackDisplayComponent::~TrackDisplayComponent() {
    // Children are automatically deleted by GraphicsComponent
}

bool TrackDisplayComponent::initialize() {
    // Set custom post-processing fragment shader (shadow vignette on sides)
    const std::string pp_frag = R"(
        #version 300 es
        precision mediump float;
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D uTexture;
        void main() {
            vec4 col = texture(uTexture, TexCoord);
            float x = TexCoord.x;
            
            // Left shadow: starts at 0.20 (closer to middle), fades to 0.0 at left edge
            // smoothstep(0.0, 0.25, x) returns 0 at x=0, 1 at x=0.25
            // So 1.0 - smoothstep gives 1 at x=0, 0 at x=0.25
            float left = 1.0 - smoothstep(0.0, 0.25, x);
            
            // Right shadow: starts at 0.80 (closer to middle), fades to 1.0 at right edge
            // smoothstep(0.75, 1.0, x) returns 0 at x=0.75, 1 at x=1.0
            float right = smoothstep(0.75, 1.0, x);
            
            float shadow_intensity = max(left, right);
            
            // Fade to transparent by reducing alpha channel
            // shadow_intensity goes from 0 (center) to 1 (edges)
            // So alpha becomes 0 at the edges (fully transparent)
            float final_alpha = col.a * (1.0 - shadow_intensity);
            
            FragColor = vec4(col.rgb, final_alpha);
        }
    )";
    
    set_post_process_fragment_shader(pp_frag);
    set_post_processing_enabled(true);

    layout_components();
    GraphicsComponent::initialize();
    return true;
}

void TrackDisplayComponent::layout_components() {
    // Calculate layout dimensions
    // We'll arrange the measure at the top, then tracks below
    
    float measure_height = m_height * 0.15f; // 15% of height for measure
    float track_row_height = (m_height - measure_height) / m_num_tracks;
    float track_width = m_width;
    
    // Get component's top-left corner position
    float comp_x, comp_y;
    get_corner_position(comp_x, comp_y);
    
    // Create measure component at the top
    m_measure_component = new TrackMeasureComponent(
        comp_x, comp_y,
        track_width, measure_height,
        PositionMode::TOP_LEFT,
        m_num_ticks
    );
    add_child(m_measure_component);
    
    // Create track row components below the measure
    for (size_t i = 0; i < m_num_tracks; ++i) {
        float track_x = comp_x;
        float track_y = comp_y - measure_height - i * track_row_height;
        
        auto track = new TrackRowComponent(
            track_x, track_y,
            track_width, track_row_height,
            PositionMode::TOP_LEFT
        );
        m_track_components.push_back(track);
        add_child(track);
    }
}

void TrackDisplayComponent::render_content() {
    // This component doesn't render anything itself,
    // it just contains child components that render
    // The base class will handle rendering children
}
