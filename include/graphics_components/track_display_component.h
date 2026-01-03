#ifndef TRACK_DISPLAY_COMPONENT_H
#define TRACK_DISPLAY_COMPONENT_H

#include <vector>
#include <memory>
#include "graphics_core/graphics_component.h"
#include "utilities/shader_program.h"

// Forward declaration
class AudioTape;

// Component that renders all the ticks (measure/ruler) directly
class TrackMeasureComponent : public GraphicsComponent {
public:
    TrackMeasureComponent(
        float x, float y, float width, float height,
        PositionMode position_mode = PositionMode::TOP_LEFT,
        float bpm = 120.0f,
        float zoom = 1.0f,
        float position_seconds = 0.0f
    );
    ~TrackMeasureComponent() override;
    
    // Set zoom level (1.0 = see full timeline, higher = more zoomed in)
    void set_zoom(float zoom);
    float get_zoom() const { return m_zoom; }
    
    // Set current position/scroll offset in seconds (where we start viewing)
    void set_position(float position_seconds);
    float get_position() const { return m_position_seconds; }
    
    // Set BPM (beats per minute) for tick spacing
    void set_bpm(float bpm);
    float get_bpm() const { return m_bpm; }
    
    // Maximum timeline duration constant (10 minutes = 600 seconds)
    static constexpr float MAX_TIMELINE_DURATION_SECONDS = 600.0f;

protected:
    bool initialize() override;
    void render_content() override;

private:
    float m_bpm; // Beats per minute
    float m_zoom; // Zoom level (1.0 = full view, higher = zoomed in)
    float m_position_seconds; // Current scroll position in seconds
    std::unique_ptr<AudioShaderProgram> m_shader_program;
    GLuint m_vao, m_vbo;
    size_t m_vertex_count;  // Always 2 (one line segment)
    
    // Calculate number of ticks to render based on visible duration and BPM
    size_t calculate_num_ticks() const;
};

// Component for rendering a single track row (horizontal bar)
class TrackRowComponent : public GraphicsComponent {
public:
    TrackRowComponent(float x, float y, float width, float height,
                     PositionMode position_mode = PositionMode::TOP_LEFT,
                     AudioTape* tape = nullptr,
                     float total_timeline_duration_seconds = 10.0f,
                     float zoom = 1.0f,
                     float position_seconds = 0.0f);
    ~TrackRowComponent() override;
    
    // Set the audio tape for this track
    void set_tape(AudioTape* tape);
    
    // Set the total timeline duration (in seconds) for mapping audio regions
    void set_timeline_duration(float duration_seconds);
    
    // Set the start offset (in seconds) where audio begins on the timeline
    void set_audio_start_offset(float start_offset_seconds);
    
    // Set selection state
    void set_selected(bool selected);
    bool is_selected() const { return m_selected; }
    
    // Set zoom level (1.0 = see full timeline, higher = more zoomed in)
    void set_zoom(float zoom);
    float get_zoom() const { return m_zoom; }
    
    // Set current position/scroll offset in seconds (where we start viewing)
    void set_position(float position_seconds);
    float get_position() const { return m_position_seconds; }
    
    // Maximum timeline duration constant (10 minutes = 600 seconds)
    static constexpr float MAX_TIMELINE_DURATION_SECONDS = 600.0f;

protected:
    bool initialize() override;
    void render_content() override;

private:
    std::unique_ptr<AudioShaderProgram> m_shader_program;
    GLuint m_vao, m_vbo;
    GLuint m_amplitude_texture; // 1D texture for amplitude data
    AudioTape* m_tape; // Pointer to audio tape (not owned)
    float m_total_timeline_duration_seconds; // Total duration of the timeline in seconds
    float m_audio_start_offset_seconds; // Start offset of audio on the timeline (default 0.0)
    float m_zoom; // Zoom level (1.0 = full view, higher = zoomed in)
    float m_position_seconds; // Current scroll position in seconds
    bool m_selected; // Whether this track is selected
    bool m_amplitude_texture_dirty; // Flag to indicate amplitude texture needs update
    
    // Dynamic update tracking
    unsigned int m_last_tape_size; // Last known tape size to detect changes
    unsigned int m_last_record_position; // Last known record position to detect changes
    unsigned int m_update_frame_counter; // Frame counter for throttling updates
    static constexpr unsigned int UPDATE_THROTTLE_FRAMES = 15; // Update every N frames
    
    void update_amplitude_texture(); // Sample audio and update texture
    bool should_update_texture() const; // Check if texture should be updated
};

// Main component that contains the measure and track rows
class TrackDisplayComponent : public GraphicsComponent {
public:
    TrackDisplayComponent(
        float x, float y, float width, float height,
        PositionMode position_mode = PositionMode::TOP_LEFT,
        size_t num_tracks = 6,
        size_t num_ticks = 10,
        float total_timeline_duration_seconds = 10.0f
    );
    ~TrackDisplayComponent() override;
    
    // Set audio tape for a specific track index
    void set_track_tape(size_t track_index, AudioTape* tape);
    
    // Set the total timeline duration (in seconds) for all tracks
    void set_timeline_duration(float duration_seconds);
    
    // Select a track (deselects all others)
    void select_track(size_t track_index);
    
    // Get the currently selected track index (returns -1 if none selected)
    int get_selected_track() const { return m_selected_track_index; }
    
    // Set zoom level for all tracks and measure (1.0 = see full timeline, higher = more zoomed in)
    void set_zoom(float zoom);
    float get_zoom() const;
    
    // Set current position/scroll offset in seconds for all tracks and measure
    void set_position(float position_seconds);
    float get_position() const;

protected:
    bool initialize() override;
    void render_content() override;

private:
    size_t m_num_tracks;
    size_t m_num_ticks;
    float m_total_timeline_duration_seconds;
    TrackMeasureComponent* m_measure_component;
    std::vector<TrackRowComponent*> m_track_components;
    int m_selected_track_index; // Currently selected track (-1 if none)
    
    // Synchronized zoom and position for all tracks and measure
    float m_zoom; // Current zoom level (synchronized across all components)
    float m_position_seconds; // Current scroll position (synchronized across all components)
    
    // Placeholder tapes for demonstration
    std::vector<std::shared_ptr<AudioTape>> m_placeholder_tapes;
    
    void layout_components();
    void create_placeholder_data();
    void synchronize_zoom_and_position(); // Internal method to sync all children
};

#endif // TRACK_DISPLAY_COMPONENT_H
