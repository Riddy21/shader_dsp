#include "graphics_views/mock_interface_view.h"
#include "graphics_components/button_component.h"
#include "audio_synthesizer/audio_synthesizer.h"
#include "audio_core/audio_renderer.h"
#include <iostream>

MockInterfaceView::MockInterfaceView() {
    auto& synthesizer = AudioSynthesizer::get_instance();
    
    // Create a Play button with AudioRenderer as context from synthesizer
    auto play_button = new ButtonComponent(
        -0.5f, 0.0f, 0.2f, 0.1f,  // Position and size
        "Play",                   // Label
        [&synthesizer]() {        // Callback
            std::cout << "Play button clicked!" << std::endl;
            synthesizer.resume();
        }
    );
    // Set custom colors for play button (green)
    play_button->set_colors(0.2f, 0.8f, 0.2f, 1.0f);
    play_button->set_hover_colors(0.3f, 0.9f, 0.3f, 1.0f);
    play_button->set_active_colors(0.1f, 0.6f, 0.1f, 1.0f);
    
    // Create a Pause button
    auto pause_button = new ButtonComponent(
        0.0f, 0.0f, 0.2f, 0.1f,   // Position and size
        "Pause",                  // Label
        [&synthesizer]() {        // Callback
            std::cout << "Pause button clicked!" << std::endl;
            synthesizer.pause();
        }
    );
    // Set custom colors for pause button (orange)
    pause_button->set_colors(0.9f, 0.6f, 0.2f, 1.0f);
    pause_button->set_hover_colors(1.0f, 0.7f, 0.3f, 1.0f);
    pause_button->set_active_colors(0.7f, 0.5f, 0.1f, 1.0f);
    
    // Create an Echo effect button
    auto echo_button = new ButtonComponent(
        0.5f, 0.0f, 0.2f, 0.1f,   // Position and size
        "Echo",                   // Label
        [&synthesizer]() {        // Callback
            std::cout << "Echo effect button clicked!" << std::endl;
            synthesizer.get_track(0).change_effect("echo");
        }
    );
    // Set custom colors for echo button (blue)
    echo_button->set_colors(0.2f, 0.4f, 0.8f, 1.0f);
    echo_button->set_hover_colors(0.3f, 0.5f, 0.9f, 1.0f);
    echo_button->set_active_colors(0.1f, 0.3f, 0.6f, 1.0f);
    
    // Create a Filter effect button
    auto filter_button = new ButtonComponent(
        0.5f, -0.2f, 0.2f, 0.1f,   // Position and size
        "Filter",                  // Label
        [&synthesizer]() {         // Callback
            std::cout << "Filter effect button clicked!" << std::endl;
            synthesizer.get_track(0).change_effect("frequency_filter");
        }
    );
    // Set custom colors for filter button (purple)
    filter_button->set_colors(0.6f, 0.2f, 0.8f, 1.0f);
    filter_button->set_hover_colors(0.7f, 0.3f, 0.9f, 1.0f);
    filter_button->set_active_colors(0.5f, 0.1f, 0.6f, 1.0f);
    
    // Add all buttons to the view
    add_component(play_button);
    add_component(pause_button);
    add_component(echo_button);
    add_component(filter_button);
}

void MockInterfaceView::register_event_handler(EventHandler* event_handler) {
    GraphicsView::register_event_handler(event_handler);

    // Get the event handler from the parent class
    if (!event_handler) {
        std::cerr << "No event handler set for MockInterfaceView" << std::endl;
        return;
    }
    
    // Register event handlers for each button component
    for (auto & component : m_components) {
        component->register_event_handlers(event_handler);
    }
}

void MockInterfaceView::unregister_event_handler(EventHandler* event_handler) {
    GraphicsView::unregister_event_handler(event_handler);
    // Unregister event handlers for each button component
    for (auto & component : m_components) {
        component->unregister_event_handlers();
    }
}