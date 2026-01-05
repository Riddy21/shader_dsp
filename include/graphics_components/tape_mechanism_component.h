#ifndef TAPE_MECHANISM_COMPONENT_H
#define TAPE_MECHANISM_COMPONENT_H

#include "graphics_core/graphics_component.h"

class ImageComponent;
class SpriteComponent;

// Component that handles tape mechanism animation (wheels and tape sprite)
// Encapsulates all the complexity of wheel rotation and sprite animation
class TapeMechanismComponent : public GraphicsComponent {
public:
    TapeMechanismComponent(
        float x, float y, float width, float height,
        PositionMode position_mode = PositionMode::CENTER
    );
    ~TapeMechanismComponent() override;
    
    // Update mechanism with current smooth position (call this each frame)
    // The component will calculate velocity and update wheel rotation/sprite animation
    void update_position(float smooth_position_seconds);
    
protected:
    bool initialize() override;
    void render_content() override;

private:
    ImageComponent* m_tape_wheel_1 = nullptr;
    ImageComponent* m_tape_wheel_2 = nullptr;
    SpriteComponent* m_tape_line_sprite = nullptr;
    
    // Position tracking for velocity calculation
    float m_previous_smooth_position = 0.0f;
    Uint32 m_last_position_update_time = 0;
    
    // Wheel rotation angle (accumulated based on smooth position changes)
    float m_wheel_rotation_angle = 0.0f;
    
    // Conversion factor: how many radians per second of position change
    // This determines how fast the wheels rotate relative to tape position
    static constexpr float POSITION_TO_ROTATION_RATE = 0.5f; // radians per second per second of position
    
    // Velocity threshold for sprite animation (seconds per second)
    static constexpr float VELOCITY_THRESHOLD = 0.01f;
};

#endif // TAPE_MECHANISM_COMPONENT_H

