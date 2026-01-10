#pragma once
#ifndef TAPE_VIEW_H
#define TAPE_VIEW_H

#include "graphics_core/graphics_view.h"
#include "graphics_core/smooth_value.h"
#include <vector>

class EqualizerComponent;
class TrackDisplayComponent;
class TapeMechanismComponent;
class TrackNumberComponent;

class TapeView : public GraphicsView {
public:
    TapeView(); // Default constructor for backward compatibility
    ~TapeView() override = default;
    
    void render() override;
    
    // Set current position/scroll offset in seconds (where we start viewing)
    void set_position(float position_seconds);
    float get_position() const;
    
    // Select a track (0-indexed, syncs with both track display and track number display)
    void select_track(int track_index);
    int get_selected_track() const;

private:
    void setup_keyboard_events(); // Set up keyboard event handlers
    
    TapeMechanismComponent* m_tape_mechanism = nullptr;
    EqualizerComponent* m_equalizer = nullptr;
    TrackDisplayComponent* m_track_display = nullptr;
    TrackNumberComponent* m_track_number_display = nullptr;
    
    // Number of tracks that exist
    int m_num_tracks = 6;
    
    // Position synchronized across track display and wheel rotation
    SmoothValue<float> m_position_seconds;
};

#endif // TAPE_VIEW_H

