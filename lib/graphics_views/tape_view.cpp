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
#include "graphics_components/track_display_component.h"
#include "graphics_core/graphics_component.h"
#include "audio_synthesizer/audio_synthesizer.h"
#include "engine/event_handler.h"
#include <SDL2/SDL.h>
#include <vector>
#include <iostream>

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
    
    // Add track display component with 6 tracks and measure with ticks
    m_track_display = new TrackDisplayComponent(
        0.0f, -0.84f,  // Position: center-top area
        1.3f, 0.34f,  // Width and height
        GraphicsComponent::PositionMode::CENTER_BOTTOM,
        6,           // 6 tracks
        10           // 10 ticks
    );
    add_component(m_track_display);
    
    // Set up keyboard event handlers
    setup_keyboard_events();
}

void TapeView::setup_keyboard_events() {
    if (!m_track_display) return;
    
    auto& event_handler = EventHandler::get_instance();
    
    // Number keys 1-6 to select tracks (0-indexed: 0-5)
    for (int i = 0; i < 6; ++i) {
        SDL_Keycode keycode = SDLK_1 + i; // SDLK_1, SDLK_2, ..., SDLK_6
        event_handler.register_entry(new KeyboardEventHandlerEntry(
            SDL_KEYDOWN, keycode,
            [this, i](const SDL_Event&) {
                if (m_track_display) {
                    m_track_display->select_track(i);
                    std::cout << "[TapeView] Selected track " << (i + 1) << std::endl;
                }
                return true;
            }
        ));
    }
    
    // Left arrow key to scroll left (decrease center position)
    event_handler.register_entry(new KeyboardEventHandlerEntry(
        SDL_KEYDOWN, SDLK_LEFT,
        [this](const SDL_Event&) {
            if (m_track_display) {
                float current_pos = m_track_display->get_position();
                float current_zoom = m_track_display->get_zoom();
                // Scroll amount depends on zoom: move by 10% of visible duration
                // Visible duration = max_duration / zoom
                // So delta = (max_duration / zoom) * 0.1
                // Assuming max duration is 600s, this scales correctly
                float visible_duration = 600.0f / current_zoom;
                float delta = visible_duration * 0.05f; // Scroll by 5% of screen width
                
                float new_pos = current_pos - delta; 
                m_track_display->set_position(new_pos); // set_position will clamp if needed
                std::cout << "[TapeView] Center position: " << new_pos << " seconds (delta: " << delta << ")" << std::endl;
            }
            return true;
        }
    ));
    
    // Right arrow key to scroll right (increase center position)
    event_handler.register_entry(new KeyboardEventHandlerEntry(
        SDL_KEYDOWN, SDLK_RIGHT,
        [this](const SDL_Event&) {
            if (m_track_display) {
                float current_pos = m_track_display->get_position();
                float current_zoom = m_track_display->get_zoom();
                float visible_duration = 600.0f / current_zoom;
                float delta = visible_duration * 0.05f; // Scroll by 5% of screen width
                
                float new_pos = current_pos + delta;
                m_track_display->set_position(new_pos); // set_position will clamp if needed
                std::cout << "[TapeView] Center position: " << new_pos << " seconds (delta: " << delta << ")" << std::endl;
            }
            return true;
        }
    ));
    
    // Up arrow key to zoom in (increase zoom)
    event_handler.register_entry(new KeyboardEventHandlerEntry(
        SDL_KEYDOWN, SDLK_UP,
        [this](const SDL_Event&) {
            if (m_track_display) {
                float current_zoom = m_track_display->get_zoom();
                float new_zoom = current_zoom * 1.2f; // Increase zoom by 20%
                m_track_display->set_zoom(new_zoom);
                std::cout << "[TapeView] Zoom: " << new_zoom << "x" << std::endl;
            }
            return true;
        }
    ));
    
    // Down arrow key to zoom out (decrease zoom)
    event_handler.register_entry(new KeyboardEventHandlerEntry(
        SDL_KEYDOWN, SDLK_DOWN,
        [this](const SDL_Event&) {
            if (m_track_display) {
                float current_zoom = m_track_display->get_zoom();
                float new_zoom = std::max(0.1f, current_zoom / 1.2f); // Decrease zoom by 20%, min 0.1x
                m_track_display->set_zoom(new_zoom);
                std::cout << "[TapeView] Zoom: " << new_zoom << "x" << std::endl;
            }
            return true;
        }
    ));
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

