#include "graphics_components/tape_mechanism_component.h"
#include "graphics_components/image_component.h"
#include "graphics_components/sprite_component.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>

TapeMechanismComponent::TapeMechanismComponent(
    float x, float y, float width, float height,
    PositionMode position_mode
) : GraphicsComponent(x, y, width, height, position_mode) {
}

TapeMechanismComponent::~TapeMechanismComponent() {
    // Children are automatically deleted by GraphicsComponent
}

bool TapeMechanismComponent::initialize() {
    
    // Create tape line sprite (animated tape visible through case window)
    std::vector<std::string> tape_line_frames = {
        "media/assets/tape_page/tape_line1.png",
        "media/assets/tape_page/tape_line2.png"
    };
    m_tape_line_sprite = new SpriteComponent(0.0f, -.89f, 2.0f, 2.03f, tape_line_frames, GraphicsComponent::PositionMode::CENTER_BOTTOM);
    m_tape_line_sprite->set_frame_rate(2.0f); // 2 frames per second
    m_tape_line_sprite->set_looping(true);
    m_tape_line_sprite->pause(); // Start paused - will play when tape moves
    add_child(m_tape_line_sprite);
    
    // Create wheel components
    m_tape_wheel_1 = new ImageComponent(-.4205f, .0933f, 1.2f, 1.2f, "media/assets/tape_page/wheel1.png", GraphicsComponent::PositionMode::CENTER);
    add_child(m_tape_wheel_1);
    
    m_tape_wheel_2 = new ImageComponent(.4187f, .0933f, 1.2f, 1.2f, "media/assets/tape_page/wheel2.png", GraphicsComponent::PositionMode::CENTER);
    add_child(m_tape_wheel_2);
    
    // Add tape case (static background) - render first so it's behind the mechanism
    auto tape_case = new ImageComponent(0.0f, 0.0f, 2.0f, 1.8f, "media/assets/tape_page/tape_case.png", GraphicsComponent::PositionMode::CENTER);
    add_child(tape_case);

    // Initialize position tracking
    m_previous_smooth_position = 0.0f;
    m_last_position_update_time = SDL_GetTicks();
    m_wheel_rotation_angle = 0.0f;
    
    GraphicsComponent::initialize();
    return true;
}

void TapeMechanismComponent::update_position(float smooth_position_seconds) {
    // Calculate position change rate for wheel rotation based on smooth position
    Uint32 current_time_ms = SDL_GetTicks();
    float dt = 0.0f;
    
    if (m_last_position_update_time > 0) {
        dt = (current_time_ms - m_last_position_update_time) / 1000.0f;
        dt = std::min(dt, 0.05f); // Clamp dt for stability
    }
    
    float smooth_position_velocity = 0.0f;
    
    if (dt > 0.0f) {
        // Calculate smooth position change rate (seconds per second)
        // This follows the displayed/smooth value, not the target value
        float smooth_position_change = smooth_position_seconds - m_previous_smooth_position;
        smooth_position_velocity = smooth_position_change / dt;
        
        // Update wheel rotation directly based on smooth position velocity
        // The wheel rotates proportionally to how fast the smooth position is changing
        float rotation_rate = smooth_position_velocity * POSITION_TO_ROTATION_RATE;
        m_wheel_rotation_angle += rotation_rate * dt;
        
        // Update previous smooth position
        m_previous_smooth_position = smooth_position_seconds;
    } else {
        // First frame or very small dt - initialize
        m_previous_smooth_position = smooth_position_seconds;
    }
    
    m_last_position_update_time = current_time_ms;
    
    // Update wheel rotations
    if (m_tape_wheel_1) {
        m_tape_wheel_1->set_rotation(m_wheel_rotation_angle);
    }
    if (m_tape_wheel_2) {
        m_tape_wheel_2->set_rotation(m_wheel_rotation_angle);
    }
    
    // Control sprite animation based on whether tape is moving
    bool is_playing = std::abs(smooth_position_velocity) > VELOCITY_THRESHOLD;
    
    if (m_tape_line_sprite) {
        if (is_playing) {
            m_tape_line_sprite->play();
        } else {
            m_tape_line_sprite->pause();
        }
    }
}

void TapeMechanismComponent::render_content() {
    // Children are automatically rendered by GraphicsComponent
    // This component doesn't render anything itself, just manages children
}

