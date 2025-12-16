#ifndef UI_BUTTON_STYLE_H
#define UI_BUTTON_STYLE_H

#include "graphics_core/ui_color_palette.h"
#include <array>

struct UIButtonStyle {
    // Structural styling (Font, Border width, etc)
    UIFontStyles::FontStyle font_style;
    float border_width;
    bool show_border;
    
    // Behavior (Alpha values for states)
    float normal_bg_alpha;
    float hover_bg_alpha;
    float active_bg_alpha;

    // Constructors
    UIButtonStyle(
        const UIFontStyles::FontStyle& font,
        float b_width = 1.0f,
        bool b_show = true,
        float normal_alpha = 0.0f,
        float hover_alpha = 0.3f,
        float active_alpha = 0.7f
    ) : font_style(font),
        border_width(b_width),
        show_border(b_show),
        normal_bg_alpha(normal_alpha),
        hover_bg_alpha(hover_alpha),
        active_bg_alpha(active_alpha) {}
};

namespace UIButtonStyles {
    // Style 1: Primary (Standard Menu)
    // - Font: Mono Regular
    // - Border: 2.0
    const UIButtonStyle PRIMARY(
        UIFontStyles::Presets::BODY_REGULAR,
        2.0f,
        true,
        0.0f, 0.3f, 0.7f
    );

    // Style 2: Secondary (Data/Status)
    // - Font: Mono Bold
    // - Border: 2.0
    const UIButtonStyle SECONDARY(
        UIFontStyles::Presets::MONO_BOLD_REGULAR,
        2.0f,
        true,
        0.0f, 0.3f, 0.7f
    );

    // Style 3: Display (Headings/Big Buttons)
    // - Font: Display Font
    // - Border: 2.0
    const UIButtonStyle DISPLAY(
        UIFontStyles::Presets::HEADING_SMALL,
        2.0f,
        true,
        0.0f, 0.3f, 0.7f
    );
    
    // Style 4: Minimal
    // - Font: Mono Thin
    // - Border: 1.0
    const UIButtonStyle MINIMAL(
        UIFontStyles::Presets::MONO_THIN_REGULAR,
        1.0f,
        true,
        0.0f, 0.3f, 0.7f
    );
}

#endif // UI_BUTTON_STYLE_H
