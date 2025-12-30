#include "graphics_views/tape_view.h"
#include "graphics_components/text_button_component.h"
#include "graphics_components/text_component.h"
#include "graphics_core/ui_font_styles.h"
#include "graphics_core/ui_color_palette.h"
#include "graphics_core/ui_button_style.h"
#include "graphics_components/image_component.h"
#include "graphics_components/sprite_component.h"
#include "graphics_components/mouse_test_component.h"
#include "graphics_components/equalizer_component.h"
#include "graphics_core/graphics_component.h"
#include "audio_synthesizer/audio_synthesizer.h"
#include <SDL2/SDL.h>
#include <vector>

TapeView::TapeView() : GraphicsView() {
    auto & audio_synthesizer = AudioSynthesizer::get_instance();
    const auto & channel_seperated_audio = audio_synthesizer.get_audio_data();
    
    std::cout << "[TapeView] Default constructor: channel_seperated_audio.size() = " 
              << channel_seperated_audio.size() << std::endl;
    
    // Convert to vector of pointers for the constructor
    std::vector<const std::vector<float>*> data_ptrs;
    for (const auto& channel : channel_seperated_audio) {
        data_ptrs.push_back(&channel);
        std::cout << "[TapeView] Channel size = " << channel.size() << std::endl;
    }

    // add sprite of tape on tape case
    std::vector<std::string> tape_line_frames = {
        "media/assets/tape_page/tape_line1.png",
        "media/assets/tape_page/tape_line2.png"
    };
    m_tape_line_sprite = new SpriteComponent(0.0f, -.89f, 2.0f, 2.03f, tape_line_frames, GraphicsComponent::PositionMode::CENTER_BOTTOM);
    m_tape_line_sprite->set_frame_rate(2.0f); // 2 frames per second
    m_tape_line_sprite->set_looping(true);
    m_tape_line_sprite->play();
    add_component(m_tape_line_sprite);
    
    // Initialize components
    m_tape_wheel_1 = new ImageComponent(-.4187f, .0933f, 1.2f, 1.2f, "media/assets/tape_page/wheel1.png", GraphicsComponent::PositionMode::CENTER);
    add_component(m_tape_wheel_1);

    m_tape_wheel_2 = new ImageComponent(.4187f, .0933f, 1.2f, 1.2f, "media/assets/tape_page/wheel2.png", GraphicsComponent::PositionMode::CENTER);
    add_component(m_tape_wheel_2);

    auto tape_component = new ImageComponent(0.0f, 0.0f, 2.0f, 1.8f, "media/assets/tape_page/tape_case.png", GraphicsComponent::PositionMode::CENTER);
    add_component(tape_component);
    
    m_equalizer = new EqualizerComponent(
        0.0f, -0.4f,  // Position: bottom-left area
        1.0f, 0.27f,    // Width: full width, Height: taller bar at bottom
        *data_ptrs[0],
        GraphicsComponent::PositionMode::CENTER_BOTTOM
    );
    add_component(m_equalizer);
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

