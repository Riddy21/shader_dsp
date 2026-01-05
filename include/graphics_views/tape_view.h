#pragma once
#ifndef TAPE_VIEW_H
#define TAPE_VIEW_H

#include "graphics_core/graphics_view.h"
#include "graphics_core/smooth_value.h"
#include <vector>

class EqualizerComponent;
class TrackDisplayComponent;
class TapeMechanismComponent;

class TapeView : public GraphicsView {
public:
    TapeView(); // Default constructor for backward compatibility
    ~TapeView() override = default;
    
    void render() override;
    
    // Set current position/scroll offset in seconds (where we start viewing)
    void set_position(float position_seconds);
    float get_position() const;

private:
    void setup_keyboard_events(); // Set up keyboard event handlers
    
    TapeMechanismComponent* m_tape_mechanism = nullptr;
    EqualizerComponent* m_equalizer = nullptr;
    TrackDisplayComponent* m_track_display = nullptr;
    
    // Position synchronized across track display and wheel rotation
    SmoothValue<float> m_position_seconds;
};

#endif // TAPE_VIEW_H

