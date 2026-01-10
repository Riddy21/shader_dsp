#ifndef TRACK_NUMBER_COMPONENT_H
#define TRACK_NUMBER_COMPONENT_H

#include "graphics_core/graphics_component.h"
#include "graphics_core/smooth_value.h"
#include "graphics_core/ui_font_styles.h"
#include "utilities/shader_program.h"
#include <vector>
#include <memory>

class TextComponent;

// Component that displays track numbers in a scroll wheel format
// Numbers scroll smoothly into and out of view when the track changes
class TrackNumberComponent : public GraphicsComponent {
public:
    TrackNumberComponent(
        float x, float y, float width, float height,
        PositionMode position_mode = PositionMode::CENTER
    );
    ~TrackNumberComponent() override;
    
    // Set the number of tracks that exist (1-indexed, e.g., 6 means tracks 1-6)
    void set_num_tracks(int num_tracks);
    
    // Get the number of tracks
    int get_num_tracks() const { return m_num_tracks; }
    
    // Select a track (1-indexed, will display as "1", "2", etc.)
    // This triggers smooth scrolling animation
    void select_track(int track);
    
    // Get the currently selected track number (1-indexed)
    int get_selected_track() const { return m_target_track; }
    
protected:
    bool initialize() override;
    void render_content() override;
    void render() override;

private:
    void update_scroll_position();
    void create_text_components();
    void initialize_tick_marks();
    std::string format_track_number(int track) const;
    
    // Text components for displaying numbers (multiple visible at once for scroll effect)
    std::vector<TextComponent*> m_text_components;
    
    // Smooth scrolling animation
    SmoothValue<float> m_scroll_position;
    
    // Target track (1-indexed)
    int m_target_track = 1;
    
    // Number of tracks that exist (1-indexed, e.g., 6 means tracks 1-6)
    int m_num_tracks = 6;
    
    // Number of visible items in the scroll wheel (should be odd for center alignment)
    // This is the maximum number of items to show, but we'll only create components for existing tracks
    static constexpr int VISIBLE_ITEMS_MAX = 5;
    
    // Actual number of visible items (min of max and num_tracks, always odd)
    int m_visible_items = 5;
    
    // Spacing between items (normalized coordinates) - horizontal spacing
    static constexpr float ITEM_SPACING = 0.15f;
    
    // Tick mark rendering
    std::unique_ptr<AudioShaderProgram> m_tick_shader;
    GLuint m_tick_vao = 0;
    GLuint m_tick_vbo = 0;
    
    // Font style for the track numbers
    UIFontStyles::FontStyle m_font_style;
};

#endif // TRACK_NUMBER_COMPONENT_H

