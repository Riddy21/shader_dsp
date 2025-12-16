#include "graphics_views/button_debug_view.h"
#include "graphics_components/text_button_component.h"
#include "graphics_components/text_component.h"
#include "graphics_core/ui_font_styles.h"
#include "graphics_core/ui_color_palette.h"
#include "graphics_core/ui_button_style.h"
#include <iostream>

ButtonDebugView::ButtonDebugView() : GraphicsView() {
    float center_x = 0.0f;
    float start_y = 0.8f;
    float spacing = 0.25f;
    float width = 0.8f;
    float height = 0.15f;

    int index = 0;

    // Helper to add a button
    auto add_button = [&](const std::string& text, const UIButtonStyle& style, const std::array<float, 4>& color) {
        float y = start_y - (index * spacing);
        auto button = new TextButtonComponent(
            center_x - width/2, y, width, height, text, style, color,
            [text]() { std::cout << "Clicked: " << text << std::endl; }
        );
        add_component(button);
        index++;
    };

    // Style 1: Primary Orange (Start Game)
    add_button("START NEW GAME", UIButtonStyles::PRIMARY, UIColorPalette::ACCENT_ORANGE);

    // Style 2: Secondary Green (Options)
    add_button("SYSTEM OPTIONS", UIButtonStyles::SECONDARY, UIColorPalette::ACCENT_GREEN);

    // Style 3: Display Amber (Load)
    add_button("LOAD SAVE DATA", UIButtonStyles::DISPLAY, UIColorPalette::ACCENT_AMBER);
    
    // Style 4: Minimal Blue (Credits)
    add_button("VIEW CREDITS", UIButtonStyles::MINIMAL, UIColorPalette::ACCENT_BLUE);
}
