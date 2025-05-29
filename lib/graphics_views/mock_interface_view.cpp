#include "graphics_views/mock_interface_view.h"
#include "graphics_components/button_component.h"
#include "graphics_components/text_button_component.h"
#include "graphics_components/image_button_component.h"
#include "graphics_components/text_component.h"
#include "graphics_components/image_component.h"
#include "audio_synthesizer/audio_synthesizer.h"
#include "audio_core/audio_renderer.h"
#include <iostream>

MockInterfaceView::MockInterfaceView(EventHandler& event_handler, const RenderContext& render_context)
    : GraphicsView(event_handler, render_context)
{
    auto& synthesizer = AudioSynthesizer::get_instance();
    
    // Define paths
    const std::string dice_image_path = "media/icons/dice.png";
    const std::string space_mono_font_path = "media/fonts/space_mono_bold.ttf";
    const std::string hack_font_path = "media/fonts/hack_regular.ttf";
    
    // Load additional fonts
    TextComponent::load_font("space_mono", space_mono_font_path);
    TextComponent::load_font("hack", hack_font_path);
    
    // Define button properties
    const float button_width = 1.f;
    const float button_height = 0.5f;

    // Create a title text at the bottom of the screen
    auto title_text = new TextComponent(
        -1.0f, 1.0f, 2.0f, 1.f, 
        "Shader DSP", 
        m_event_handler, 
        m_render_context
    );
    title_text->set_font("space_mono"); // Use Space Mono for title
    title_text->set_text_color(1.0f, 1.0f, 1.0f, 1.0f);
    title_text->set_horizontal_alignment(0.5f); // Centered horizontally
    title_text->set_vertical_alignment(0.5f);   // Centered vertically
    add_component(title_text);

    // Define button information
    struct ButtonInfo {
        std::string label;
        std::function<void()> callback;
        float color[4];
        float hover_color[4];
        float active_color[4];
        bool use_image; // Whether to use image instead of text
        std::string font_name; // Font to use for text buttons
    };

    std::vector<ButtonInfo> buttons = {
        {"Play", [&synthesizer]() {
            std::cout << "Play button clicked!" << std::endl;
            synthesizer.resume();
        }, {0.2f, 0.8f, 0.2f, 1.0f}, {0.3f, 0.9f, 0.3f, 1.0f}, {0.1f, 0.6f, 0.1f, 1.0f}, false, "space_mono"},

        {"Pause", [&synthesizer]() {
            std::cout << "Pause button clicked!" << std::endl;
            synthesizer.pause();
        }, {0.9f, 0.6f, 0.2f, 1.0f}, {1.0f, 0.7f, 0.3f, 1.0f}, {0.7f, 0.5f, 0.1f, 1.0f}, false, "hack"},

        {"Echo", [&synthesizer]() {
            std::cout << "Echo effect button clicked!" << std::endl;
            synthesizer.get_track(0).change_effect("echo");
        }, {0.2f, 0.4f, 0.8f, 1.0f}, {0.3f, 0.5f, 0.9f, 1.0f}, {0.1f, 0.3f, 0.6f, 1.0f}, true, "default"},

        {"Filter", [&synthesizer]() {
            std::cout << "Filter effect button clicked!" << std::endl;
            synthesizer.get_track(0).change_effect("frequency_filter");
        }, {0.6f, 0.2f, 0.8f, 1.0f}, {0.7f, 0.3f, 0.9f, 1.0f}, {0.5f, 0.1f, 0.6f, 1.0f}, true, "default"}
    };

    // Calculate grid layout to span the second half of the screen
    int columns = 2;
    float screen_width = 2.0f;  // Normalized device coordinates span from -1 to 1 (width of 2)
    float screen_height = 2.0f; // Height from -1 to 1 is also 2 units
    
    // Define the starting point for the second half of the screen
    float start_x = -1.0f; // Start at the middle of the screen
    float start_y = 0.0f; // Start at the bottom of the screen

    // Arrange buttons in two rows spanning the second half of the screen
    for (size_t i = 0; i < buttons.size(); ++i) {
        float x, y;
        
        // Calculate position based on row and column
        x = start_x + (i % columns) * button_width; // Horizontal position
        y = start_y - (i / columns) * button_height; // Vertical position

        auto& info = buttons[i];
        
        if (info.use_image) {
            // Create an image button with event handler and render context
            auto button = new ImageButtonComponent(
                x, y, button_width, button_height,
                dice_image_path,
                info.callback,
                m_event_handler,
                m_render_context
            );

            // Set button colors
            button->set_colors(info.color[0], info.color[1], info.color[2], info.color[3]);
            button->set_hover_colors(info.hover_color[0], info.hover_color[1], info.hover_color[2], info.hover_color[3]);
            button->set_active_colors(info.active_color[0], info.active_color[1], info.active_color[2], info.active_color[3]);
            
            // Set image tint colors
            button->set_tint_color(1.0f, 1.0f, 1.0f, 0.9f);
            button->set_hover_tint_color(1.0f, 1.0f, 1.0f, 1.0f);
            button->set_active_tint_color(0.8f, 0.8f, 0.8f, 0.9f);
            
            // Set image scaling
            button->set_scale_mode(ImageComponent::ScaleMode::CONTAIN);
            
            add_component(button);
        } else {
            // Create a text button with event handler and render context
            auto button = new TextButtonComponent(
                x, y, button_width, button_height,
                info.label,
                info.callback,
                m_event_handler,
                m_render_context
            );

            // Set button colors
            button->set_colors(info.color[0], info.color[1], info.color[2], info.color[3]);
            button->set_hover_colors(info.hover_color[0], info.hover_color[1], info.hover_color[2], info.hover_color[3]);
            button->set_active_colors(info.active_color[0], info.active_color[1], info.active_color[2], info.active_color[3]);
            
            // Set text colors to be white with varying brightness
            button->set_text_color(1.0f, 1.0f, 1.0f, 1.0f);
            button->set_hover_text_color(1.0f, 1.0f, 1.0f, 1.0f);
            button->set_active_text_color(0.8f, 0.8f, 0.8f, 1.0f);
            
            // Configure text properties with the specified font
            button->set_font(info.font_name);
            button->set_font_size(20);
            
            add_component(button);
        }
    }
}