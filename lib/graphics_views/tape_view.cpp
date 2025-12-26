#include "graphics_views/tape_view.h"
#include "graphics_components/text_button_component.h"
#include "graphics_components/text_component.h"
#include "graphics_core/ui_font_styles.h"
#include "graphics_core/ui_color_palette.h"
#include "graphics_core/ui_button_style.h"
#include "graphics_components/image_component.h"
#include "graphics_components/mouse_test_component.h"
#include "graphics_core/graphics_component.h"
#include <SDL2/SDL.h>

TapeView::TapeView() : GraphicsView() {
    m_tape_wheel_1 = new ImageComponent(GraphicsComponent::PositionMode::CENTER, -.4187f, .0833f, 1.2f, 1.2f, "media/assets/tape_page/wheel1.png");
    add_component(m_tape_wheel_1);

    m_tape_wheel_2 = new ImageComponent(GraphicsComponent::PositionMode::CENTER, .4187f, .0833f, 1.2f, 1.2f, "media/assets/tape_page/wheel2.png");
    add_component(m_tape_wheel_2);

    auto tape_component = new ImageComponent(GraphicsComponent::PositionMode::CENTER, 0.0f, 0.0f, 2.0f, 1.8f, "media/assets/tape_page/tape_case.png");
    add_component(tape_component);
}

void TapeView::render() {
    // Calculate rotation based on time
    Uint32 current_time_ms = SDL_GetTicks();
    float current_time_seconds = current_time_ms / 1000.0f;
    float rotation_angle = current_time_seconds * m_rotation_speed;
    
    // Update wheel rotations
    if (m_tape_wheel_1) {
        m_tape_wheel_1->set_rotation(rotation_angle);
    }
    if (m_tape_wheel_2) {
        m_tape_wheel_2->set_rotation(rotation_angle);
    }
    
    // Call base class render to render all components
    GraphicsView::render();
}

