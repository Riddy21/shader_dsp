#include "graphics_views/tape_view.h"
#include "graphics_components/text_button_component.h"
#include "graphics_components/text_component.h"
#include "graphics_core/ui_font_styles.h"
#include "graphics_core/ui_color_palette.h"
#include "graphics_core/ui_button_style.h"
#include "graphics_components/image_component.h"
#include "graphics_components/tape_mechanism_component.h"
#include "graphics_components/mouse_test_component.h"
#include "graphics_components/equalizer_component.h"
#include "graphics_components/track_display_component.h"
#include "graphics_core/graphics_component.h"
#include "audio_synthesizer/audio_synthesizer.h"
#include "engine/event_handler.h"
#include <SDL2/SDL.h>
#include <vector>
#include <algorithm>

// TOOD: Add some features for tape speed and record speed

TapeView::TapeView() : GraphicsView(), m_position_seconds(0.0f, 8.0f, 1.0f) {
    auto & audio_synthesizer = AudioSynthesizer::get_instance();
    const auto & channel_seperated_audio = audio_synthesizer.get_audio_data();
    
    // Convert to vector of pointers for the constructor
    std::vector<const std::vector<float>*> data_ptrs;
    for (const auto& channel : channel_seperated_audio) {
        data_ptrs.push_back(&channel);
    }

    // Create tape mechanism component (includes case, wheels, and tape sprite)
    m_tape_mechanism = new TapeMechanismComponent(0.0f, 0.0f, 2.0f, 1.8f, GraphicsComponent::PositionMode::CENTER);
    add_component(m_tape_mechanism);
    
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
                float current_pos = get_position();
                float current_zoom = m_track_display->get_zoom();
                // Scroll amount depends on zoom: move by 10% of visible duration
                // Visible duration = max_duration / zoom
                // So delta = (max_duration / zoom) * 0.1
                // Assuming max duration is 600s, this scales correctly
                float visible_duration = 600.0f / current_zoom;
                float delta = visible_duration * 0.05f; // Scroll by 5% of screen width
                
                float new_pos = current_pos - delta; 
                set_position(new_pos); // set_position will clamp if needed
            }
            return true;
        }
    ));
    
    // Right arrow key to scroll right (increase center position)
    event_handler.register_entry(new KeyboardEventHandlerEntry(
        SDL_KEYDOWN, SDLK_RIGHT,
        [this](const SDL_Event&) {
            if (m_track_display) {
                float current_pos = get_position();
                float current_zoom = m_track_display->get_zoom();
                float visible_duration = 600.0f / current_zoom;
                float delta = visible_duration * 0.05f; // Scroll by 5% of screen width
                
                float new_pos = current_pos + delta;
                set_position(new_pos); // set_position will clamp if needed
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
                float new_zoom = current_zoom / 1.2f; // Decrease zoom by 20%
                m_track_display->set_zoom(new_zoom); // set_zoom will clamp to min 1.0
            }
            return true;
        }
    ));
}

void TapeView::set_position(float position_seconds) {
    // Position represents the center of the viewport (position 0 = center at time 0)
    // Clamp position to stay within tape limits (0 to MAX_TIMELINE_DURATION_SECONDS)
    if (position_seconds >= 0.0f) {
        constexpr float MAX_TIMELINE_DURATION_SECONDS = 600.0f; // 10 minutes
        float clamped_pos = std::max(0.0f, std::min(position_seconds, MAX_TIMELINE_DURATION_SECONDS));
        m_position_seconds.set_target(clamped_pos);
        
        // Update track display component position
        if (m_track_display) {
            m_track_display->set_position(clamped_pos);
        }
    }
}

float TapeView::get_position() const {
    return m_position_seconds.get_target();
}

void TapeView::render() {
    // Update position smooth transition
    m_position_seconds.update();
    
    // Get current smooth/displayed position value (this is what's visually shown)
    float current_smooth_position = m_position_seconds.get_current();
    
    // Update tape mechanism with smooth position (handles wheel rotation and sprite animation)
    if (m_tape_mechanism) {
        m_tape_mechanism->update_position(current_smooth_position);
    }
    
    // Call base class render to render all components
    GraphicsView::render();
}

