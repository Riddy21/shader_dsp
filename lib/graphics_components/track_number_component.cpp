#include "graphics_components/track_number_component.h"
#include "graphics_components/text_component.h"
#include "graphics_core/ui_color_palette.h"
#include "utilities/shader_program.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <SDL2/SDL.h>

TrackNumberComponent::TrackNumberComponent(
    float x, float y, float width, float height,
    PositionMode position_mode
) : GraphicsComponent(x, y, width, height, position_mode),
    m_scroll_position(1.0f, 12.0f, 1.0f), // Smooth scrolling with frequency 12, damping 1.0, start at track 1
    m_target_track(1),
    m_num_tracks(6), // Default to 6 tracks
    m_visible_items(5), // Default visible items
    m_font_style(UIFontStyles::LIBERTAD_BOLD, UIFontStyles::FontSize::SMALL, 0.5f, 0.5f) // Small bold font, center aligned
{
    // Calculate initial visible items
    m_visible_items = std::min(VISIBLE_ITEMS_MAX, m_num_tracks);
    // Ensure visible items is odd for center alignment
    if (m_visible_items > 0 && m_visible_items % 2 == 0) {
        m_visible_items = std::max(1, m_visible_items - 1);
    }
}

TrackNumberComponent::~TrackNumberComponent() {
    // Text components are children and will be automatically deleted by GraphicsComponent
    // Clean up tick mark OpenGL resources
    if (m_tick_vao != 0) {
        glDeleteVertexArrays(1, &m_tick_vao);
    }
    if (m_tick_vbo != 0) {
        glDeleteBuffers(1, &m_tick_vbo);
    }
}

bool TrackNumberComponent::initialize() {
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
            
            // Left shadow: starts at 0.15 (closer to middle), fades to 0.0 at left edge
            // smoothstep(0.0, 0.25, x) returns 0 at x=0, 1 at x=0.25
            // So 1.0 - smoothstep gives 1 at x=0, 0 at x=0.25
            float left = 1.0 - smoothstep(0.0, 0.25, x);
            
            // Right shadow: starts at 0.85 (closer to middle), fades to 1.0 at right edge
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
    
    create_text_components();
    initialize_tick_marks();
    // Initialize scroll position to match target track
    m_scroll_position.snap_to_target();
    // Update initial display
    update_scroll_position();
    GraphicsComponent::initialize();
    return true;
}

void TrackNumberComponent::create_text_components() {
    // Clear existing components
    for (auto* text : m_text_components) {
        remove_child(text);
        delete text;
    }
    m_text_components.clear();
    
    // Create text components only for the number of tracks that exist
    // We'll show m_visible_items numbers at once, with the center one being the current track
    // Calculate spacing and sizes relative to parent dimensions
    float item_spacing = 0.25f * m_width; // Spacing so 5 items roughly fill the width
    float base_text_width = 0.25f * m_width; // Text width 25% of component width
    float base_text_height = 0.4f * m_height; // Text height 40% of component height
    
    for (int i = 0; i < m_visible_items; ++i) {
        // Calculate horizontal offset for this item relative to parent center
        // Center item (i == (m_visible_items-1)/2) is at x=0
        float offset = (i - (m_visible_items - 1) / 2.0f) * item_spacing;
        
        TextComponent* text = new TextComponent(
            offset, 0.0f,  // Horizontal position, centered vertically (relative to parent center)
            base_text_width, base_text_height,
            "1",
            m_font_style,
            GraphicsComponent::PositionMode::CENTER
        );
        text->set_text_color(UIColorPalette::PRIMARY_ORANGE);
        text->set_antialiased(false); // Use aliased text for pixelated look
        add_child(text);
        m_text_components.push_back(text);
    }
}

void TrackNumberComponent::set_num_tracks(int num_tracks) {
    if (num_tracks < 1) num_tracks = 1;
    if (num_tracks > 99) num_tracks = 99;
    
    if (m_num_tracks != num_tracks) {
        m_num_tracks = num_tracks;
        // Recalculate visible items
        m_visible_items = std::min(VISIBLE_ITEMS_MAX, m_num_tracks);
        // Ensure visible items is odd for center alignment
        if (m_visible_items > 0 && m_visible_items % 2 == 0) {
            m_visible_items = std::max(1, m_visible_items - 1);
        }
        
        // Clamp target track to valid range
        if (m_target_track > m_num_tracks) {
            m_target_track = m_num_tracks;
            m_scroll_position.set_target(static_cast<float>(m_target_track));
        }
        
        // Recreate text components if already initialized
        if (m_initialized) {
            create_text_components();
            // Reinitialize tick marks buffer size
            if (m_tick_vao != 0 && m_tick_vbo != 0) {
                glBindBuffer(GL_ARRAY_BUFFER, m_tick_vbo);
                glBufferData(GL_ARRAY_BUFFER, m_visible_items * 4 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
            }
        }
    }
}

void TrackNumberComponent::select_track(int track) {
    // Allow track 0 for "--" display, otherwise clamp to 1-m_num_tracks
    if (track < 0) track = 0;
    if (track > m_num_tracks) track = m_num_tracks;
    
    if (m_target_track != track) {
        m_target_track = track;
        // Set scroll position to the new track (will animate smoothly)
        // For track 0, scroll to 0.0 (will display as "--")
        float target_scroll = (track == 0) ? 0.0f : static_cast<float>(track);
        m_scroll_position.set_target(target_scroll);
    }
}

void TrackNumberComponent::initialize_tick_marks() {
    // Shader for drawing tick marks (small vertical lines)
    const std::string vertex_shader_src = R"(
        #version 300 es
        layout (location = 0) in vec2 aPos;  // Position in global NDC space (relative to component center)
        
        void main() {
            // Tick marks are small vertical lines at each number position
            // Coordinates are in global NDC space (same as text components)
            gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
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
    
    m_tick_shader = std::make_unique<AudioShaderProgram>(vertex_shader_src, fragment_shader_src);
    if (!m_tick_shader->initialize()) {
        std::cerr << "Failed to initialize tick mark shader program" << std::endl;
        return;
    }
    
    // Create VAO and VBO for tick marks (vertical lines)
    glGenVertexArrays(1, &m_tick_vao);
    glGenBuffers(1, &m_tick_vbo);
    
    // Each tick is a vertical line (we'll update positions dynamically)
    // Format: [x, top_y, x, bottom_y] for each tick
    // We'll allocate space for VISIBLE_ITEMS ticks
    glBindVertexArray(m_tick_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_tick_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_visible_items * 4 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

std::string TrackNumberComponent::format_track_number(int track) const {
    if (track < 1 || track > 99) {
        return "--";
    }
    return std::to_string(track);
}


void TrackNumberComponent::update_scroll_position() {
    // Update smooth scrolling animation
    m_scroll_position.update();
    
    // Get current scroll position (smoothly animated)
    float current_scroll = m_scroll_position.get_current();
    
    // Calculate which tracks to display around the current scroll position
    // We want to show tracks centered around current_scroll
    int center_track = static_cast<int>(std::floor(current_scroll + 0.5f));
    
    // Clamp center_track to valid range
    if (center_track < 1) center_track = 1;
    if (center_track > m_num_tracks) center_track = m_num_tracks;
    
    // The center index is always the middle item (m_visible_items / 2)
    // This represents the center_track value, ensuring consistent highlighting
    size_t center_index = m_visible_items / 2;
    
    // Calculate positions for all items relative to parent dimensions
    float item_spacing = 0.25f * m_width; // Match the spacing used in creation
    std::vector<float> x_positions;
    x_positions.reserve(m_text_components.size());
    
    for (size_t i = 0; i < m_text_components.size(); ++i) {
        float base_offset = (static_cast<float>(i) - (m_visible_items - 1) / 2.0f) * item_spacing;
        float scroll_delta = current_scroll - static_cast<float>(center_track);
        float x_pos = base_offset - (scroll_delta * item_spacing);
        x_positions.push_back(x_pos);
    }
    
    // Update components with highlighting for center track only
    for (size_t i = 0; i < m_text_components.size(); ++i) {
        int offset = static_cast<int>(i) - m_visible_items / 2;
        int display_track = center_track + offset;
        
        // Only show tracks that exist (1 to m_num_tracks)
        if (display_track < 1 || display_track > m_num_tracks) {
            // Hide this component by making it invisible
            if (m_text_components[i]) {
                m_text_components[i]->set_text(""); // Empty text
                m_text_components[i]->set_dimensions(0.0f, 0.0f); // Zero size
            }
            continue;
        }
        float x_pos = x_positions[i];
        
        // Update text component position and content
        if (m_text_components[i]) {
            std::string text = format_track_number(display_track);
            if (m_text_components[i]->get_text() != text) {
                m_text_components[i]->set_text(text);
            }
            
            // Only the center track (middle item) is highlighted (bigger)
            bool is_center = (i == center_index);
            
            float scale;
            float alpha;
            
            if (is_center) {
                // Center track: bigger and fully opaque
                scale = 1.5f; // Center track is 1.5x bigger
                alpha = 1.0f;
            } else {
                // Other tracks: perspective scaling based on distance from center
                float distance_from_center = std::abs(x_pos);
                scale = 1.0f - (distance_from_center * 0.6f); // Scale down as distance increases
                scale = std::max(0.3f, std::min(1.0f, scale)); // Clamp between 0.3 and 1.0
                
                alpha = 1.0f - (distance_from_center * 1.2f);
                alpha = std::max(0.2f, std::min(1.0f, alpha)); // Clamp between 0.2 and 1.0
            }
            
            // Apply scaling first - base sizes are relative to parent dimensions
            float base_width = 0.25f * m_width;
            float base_height = 0.4f * m_height;
            m_text_components[i]->set_dimensions(base_width * scale, base_height * scale);
            
            // Then set position (after dimensions are set, so center is correctly positioned)
            // PositionMode::CENTER means set_position sets the center point
            // Get parent's center position and add relative offset
            float parent_x, parent_y;
            get_center_position(parent_x, parent_y);
            m_text_components[i]->set_position(parent_x + x_pos, parent_y + 0.0f);
            
            // Update text color with alpha
            const auto& color = UIColorPalette::PRIMARY_ORANGE;
            m_text_components[i]->set_text_color(color[0], color[1], color[2], alpha);
        }
    }
    
    // Update tick mark positions (using the same x_positions calculated above)
    // Convert to local NDC space (-1 to 1) for rendering in render_content()
    // Children render after end_local_rendering() so they use global NDC, but they're still in the FBO
    // We render ticks in render_content() which is within local viewport, so use local NDC
    if (m_tick_vao != 0 && m_tick_vbo != 0) {
        std::vector<float> tick_vertices;
        tick_vertices.reserve(m_visible_items * 4); // 2 vertices per tick, 2 floats per vertex
        
        // Convert from global NDC (relative to component center) to local NDC (within component viewport)
        // Component has width m_width and height m_height, so divide by half-dimensions
        float scale_x = 2.0f / m_width;
        float scale_y = 2.0f / m_height;
        
        // Scale tick mark vertical positions relative to parent height
        // Text is centered at 0.0. Place ticks above the text.
        float tick_bottom_offset = 0.25f * m_height; // Start above the text (in global NDC)
        float tick_top_offset = 0.40f * m_height;   // End near the top edge (in global NDC)
        
        for (size_t i = 0; i < m_text_components.size(); ++i) {
            int offset = static_cast<int>(i) - m_visible_items / 2;
            int display_track = center_track + offset;
            
            // Only draw ticks for valid tracks that exist
            if (display_track >= 1 && display_track <= m_num_tracks) {
                float x_pos = x_positions[i]; // x_pos is in global NDC relative to component center
                
                // Convert to local NDC space for rendering in render_content()
                float local_x = x_pos * scale_x;
                float local_y_bottom = tick_bottom_offset * scale_y;
                float local_y_top = tick_top_offset * scale_y;
                
                tick_vertices.push_back(local_x);
                tick_vertices.push_back(local_y_bottom);
                tick_vertices.push_back(local_x);
                tick_vertices.push_back(local_y_top);
            }
        }
        
        // Update VBO with new tick positions
        if (!tick_vertices.empty()) {
            glBindBuffer(GL_ARRAY_BUFFER, m_tick_vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, tick_vertices.size() * sizeof(float), tick_vertices.data());
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }
}

void TrackNumberComponent::render_content() {
    // Update scroll position animation
    update_scroll_position();
    
    // Render tick marks (so they're included in the FBO for shadow effect)
    // Tick marks use global NDC coordinates (same as text components)
    if (m_tick_shader && m_tick_vao != 0) {
        glUseProgram(m_tick_shader->get_program());
        
        // Set tick color (same as text color)
        const auto& color = UIColorPalette::PRIMARY_ORANGE;
        GLuint color_loc = glGetUniformLocation(m_tick_shader->get_program(), "uColor");
        if (color_loc != -1) {
            glUniform4f(color_loc, color[0], color[1], color[2], color[3] * 0.8f); // Slightly more transparent
        }
        
        // Enable blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        // Draw tick marks as lines
        glBindVertexArray(m_tick_vao);
        glLineWidth(0.8f);
        
        // Count how many ticks we have (each tick is 2 vertices)
        int tick_count = 0;
        float current_scroll = m_scroll_position.get_current();
        int center_track = static_cast<int>(std::floor(current_scroll + 0.5f));
        if (center_track < 1) center_track = 1;
        if (center_track > m_num_tracks) center_track = m_num_tracks;
        
        for (size_t i = 0; i < m_text_components.size(); ++i) {
            int offset = static_cast<int>(i) - m_visible_items / 2;
            int display_track = center_track + offset;
            if (display_track >= 1 && display_track <= m_num_tracks) {
                tick_count++;
            }
        }
        
        if (tick_count > 0) {
            glDrawArrays(GL_LINES, 0, tick_count * 2);
        }
        
        glBindVertexArray(0);
        glDisable(GL_BLEND);
        glUseProgram(0);
    }
    
    // Children (text components) are automatically rendered by GraphicsComponent
}

void TrackNumberComponent::render() {
    // Call base render which handles FBO setup, render_content(), and post-processing
    GraphicsComponent::render();
}

