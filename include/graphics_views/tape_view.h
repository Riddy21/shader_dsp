#pragma once
#ifndef TAPE_VIEW_H
#define TAPE_VIEW_H

#include "graphics_core/graphics_view.h"
#include "graphics_components/image_component.h"
#include <vector>

class EqualizerComponent;
class SpriteComponent;
class TrackDisplayComponent;

class TapeView : public GraphicsView {
public:
    TapeView(); // Default constructor for backward compatibility
    ~TapeView() override = default;
    
    void render() override;

private:
    void setup_keyboard_events(); // Set up keyboard event handlers
    
    ImageComponent* m_tape_wheel_1 = nullptr;
    ImageComponent* m_tape_wheel_2 = nullptr;
    SpriteComponent* m_tape_line_sprite = nullptr;
    EqualizerComponent* m_equalizer = nullptr;
    TrackDisplayComponent* m_track_display = nullptr;
    float m_rotation_speed = 1.0f; // Rotation speed in radians per second
};

#endif // TAPE_VIEW_H

