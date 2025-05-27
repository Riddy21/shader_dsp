#include "graphics_views/mock_interface_view.h"
#include "graphics_components/button_component.h"
#include "audio_synthesizer/audio_synthesizer.h"
#include "audio_core/audio_renderer.h"
#include <iostream>

MockInterfaceView::MockInterfaceView()
    : GraphicsView()
{
    auto& synthesizer = AudioSynthesizer::get_instance();
    
    // Define button properties
    const float button_width = 0.1f;
    const float button_height = 0.1f;

    // Define button labels, callbacks, and colors
    struct ButtonInfo {
        std::string label;
        std::function<void()> callback;
        float color[4];
        float hover_color[4];
        float active_color[4];
    };

    std::vector<ButtonInfo> buttons = {
        {"Play", [&synthesizer]() {
            std::cout << "Play button clicked!" << std::endl;
            synthesizer.resume();
        }, {0.2f, 0.8f, 0.2f, 1.0f}, {0.3f, 0.9f, 0.3f, 1.0f}, {0.1f, 0.6f, 0.1f, 1.0f}},

        {"Pause", [&synthesizer]() {
            std::cout << "Pause button clicked!" << std::endl;
            synthesizer.pause();
        }, {0.9f, 0.6f, 0.2f, 1.0f}, {1.0f, 0.7f, 0.3f, 1.0f}, {0.7f, 0.5f, 0.1f, 1.0f}},

        {"Echo", [&synthesizer]() {
            std::cout << "Echo effect button clicked!" << std::endl;
            synthesizer.get_track(0).change_effect("echo");
        }, {0.2f, 0.4f, 0.8f, 1.0f}, {0.3f, 0.5f, 0.9f, 1.0f}, {0.1f, 0.3f, 0.6f, 1.0f}},

        {"Filter", [&synthesizer]() {
            std::cout << "Filter effect button clicked!" << std::endl;
            synthesizer.get_track(0).change_effect("frequency_filter");
        }, {0.6f, 0.2f, 0.8f, 1.0f}, {0.7f, 0.3f, 0.9f, 1.0f}, {0.5f, 0.1f, 0.6f, 1.0f}}
    };

    // Calculate grid layout to span the whole screen
    int columns = 2;
    float screen_width = 1.0f;  // Assuming normalized device coordinates (-1 to 1)
    float screen_height = 1.0f;
    float x_spacing = (screen_width - columns * button_width) / (columns + 1);
    float y_spacing = (screen_height - (buttons.size() / columns) * button_height) / ((buttons.size() / columns) + 1);

    for (size_t i = 0; i < buttons.size(); ++i) {
        float x = -0.5f + x_spacing + (i % columns) * (button_width + x_spacing);
        float y = 0.5f - y_spacing - (i / columns) * (button_height + y_spacing);

        auto& info = buttons[i];
        auto button = new ButtonComponent(
            x, y, button_width, button_height,
            info.label,
            info.callback
        );

        button->set_colors(info.color[0], info.color[1], info.color[2], info.color[3]);
        button->set_hover_colors(info.hover_color[0], info.hover_color[1], info.hover_color[2], info.hover_color[3]);
        button->set_active_colors(info.active_color[0], info.active_color[1], info.active_color[2], info.active_color[3]);

        add_component(button);
    }
}