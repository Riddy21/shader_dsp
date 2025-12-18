#include "graphics_views/style_debug_view.h"
#include "graphics_components/text_component.h"
#include "graphics_core/ui_color_palette.h"
#include "graphics_core/ui_font_styles.h"

StyleDebugView::StyleDebugView()
    : GraphicsView()
{
    // Create text components to display various styles
    
    // Helper lambda to create and add a styled text component
    auto component = new TextComponent(-1.0f, 1.0f, 2.0f, 1.0f, "record", UIFontStyles::Presets::TITLE);
    component->set_text_color(UIColorPalette::ACCENT_AMBER);
    add_component(component);

    auto component2 = new TextComponent(-1.0f, 0.0f, 2.0f, 1.0f, "play", UIFontStyles::Presets::SUBTITLE);
    component2->set_text_color(UIColorPalette::ACCENT_AMBER);
    add_component(component2);
    
}
