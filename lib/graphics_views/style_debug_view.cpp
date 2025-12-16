#include "graphics_views/style_debug_view.h"
#include "graphics_components/text_component.h"
#include "graphics_core/ui_color_palette.h"
#include "graphics_core/ui_font_styles.h"

StyleDebugView::StyleDebugView()
    : GraphicsView()
{
    // Create text components to display various styles
    // Layout: Each text component will be positioned vertically
    
    float start_y = 0.95f;
    float line_height = 0.08f;
    float width = 1.8f;
    float height = 0.08f;
    float x = -0.9f;
    
    int line_index = 0;
    
    // Helper lambda to create and add a styled text component
    auto add_styled_text = [&](const std::string& text, const UIFontStyles::FontStyle& font, const std::array<float, 4>& color) {
        float y = start_y - (line_index * line_height);
        auto component = new TextComponent(x, y, width, height, text, font, color);
        component->set_vertical_alignment(0.0f);
        component->set_horizontal_alignment(0.0f);
        add_component(component);
        line_index++;
    };
    
    // Helper lambda to create a section header
    auto create_section_header = [&](const std::string& title) {
        line_index++; // Spacer before section
        add_styled_text("=== " + title + " ===", 
            UIFontStyles::FontStyle(UIFontStyles::FONT_MONO_BOLD, UIFontStyles::FontSize::SMALL), 
            UIColorPalette::ACCENT_AMBER);
    };
    
    // ===== HEADING STYLES =====
    create_section_header("HEADING STYLES");
    add_styled_text("Heading Primary Large Display Font", UIFontStyles::Presets::HEADING_LARGE, UIColorPalette::TEXT_BRIGHT);
    add_styled_text("Heading Secondary Medium Display Font", UIFontStyles::Presets::HEADING_MEDIUM, UIColorPalette::TEXT_PRIMARY);
    add_styled_text("Heading Tertiary Small Display Font", UIFontStyles::Presets::HEADING_SMALL, UIColorPalette::TEXT_SECONDARY);
    
    // ===== BODY TEXT STYLES =====
    create_section_header("BODY TEXT STYLES");
    add_styled_text("Body Large - Large Monospace", UIFontStyles::Presets::BODY_LARGE, UIColorPalette::TEXT_PRIMARY);
    add_styled_text("Body Primary - Regular Monospace", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::TEXT_PRIMARY);
    add_styled_text("Body Secondary - Small Monospace", UIFontStyles::Presets::BODY_SMALL, UIColorPalette::TEXT_SECONDARY);
    add_styled_text("Body Muted - Small Muted Color", UIFontStyles::Presets::BODY_SMALL, UIColorPalette::TEXT_MUTED);
    
    // ===== MONOSPACE BOLD STYLES =====
    create_section_header("MONOSPACE BOLD STYLES");
    add_styled_text("Mono Bold Large - Bold Large", UIFontStyles::Presets::MONO_BOLD_LARGE, UIColorPalette::TEXT_BRIGHT);
    add_styled_text("Mono Bold Secondary - Bold Medium", UIFontStyles::Presets::MONO_BOLD_MEDIUM, UIColorPalette::TEXT_BRIGHT);
    add_styled_text("Mono Bold Primary - Bold Regular", UIFontStyles::Presets::MONO_BOLD_REGULAR, UIColorPalette::TEXT_PRIMARY);
    
    // ===== MONOSPACE THIN STYLES =====
    create_section_header("MONOSPACE THIN STYLES");
    add_styled_text("Mono Thin Primary - Thin Regular", UIFontStyles::Presets::MONO_THIN_REGULAR, UIColorPalette::TEXT_SECONDARY);
    add_styled_text("Mono Thin Small - Thin Small", UIFontStyles::Presets::MONO_THIN_SMALL, UIColorPalette::TEXT_MUTED);
    
    // ===== SPECIAL PURPOSE STYLES =====
    create_section_header("SPECIAL PURPOSE STYLES");
    add_styled_text("Button Text - Regular Bright", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::TEXT_BRIGHT);
    add_styled_text("Label Style - Small Secondary", UIFontStyles::Presets::BODY_SMALL, UIColorPalette::TEXT_SECONDARY);
    add_styled_text("Caption Style - Small Muted", UIFontStyles::Presets::BODY_SMALL, UIColorPalette::TEXT_MUTED);
    add_styled_text("Disabled Text - Small Disabled", UIFontStyles::Presets::BODY_SMALL, UIColorPalette::TEXT_DISABLED);
    
    // ===== ACCENT COLOR STYLES =====
    create_section_header("ACCENT COLOR STYLES");
    add_styled_text("Accent Yellow - Regular Yellow", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::ACCENT_YELLOW);
    add_styled_text("Accent Orange - Regular Orange", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::ACCENT_ORANGE);
    add_styled_text("Accent Coral - Regular Coral", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::ACCENT_CORAL);
    add_styled_text("Accent Green - Regular Green", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::ACCENT_GREEN);
    add_styled_text("Accent Blue - Regular Blue", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::ACCENT_BLUE);
    add_styled_text("Accent Amber - Regular Amber", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::ACCENT_AMBER);
    
    // ===== COLOR PALETTE SAMPLES =====
    create_section_header("COLOR PALETTE SAMPLES");
    
    // Primary colors
    add_styled_text("Primary Yellow (#F7D14E)", UIFontStyles::Presets::BODY_SMALL, UIColorPalette::PRIMARY_YELLOW);
    add_styled_text("Primary Orange (#FF825C)", UIFontStyles::Presets::BODY_SMALL, UIColorPalette::PRIMARY_ORANGE);
    add_styled_text("Primary Green (#88FF9C)", UIFontStyles::Presets::BODY_SMALL, UIColorPalette::PRIMARY_GREEN);
    add_styled_text("Primary Blue (#3378C7)", UIFontStyles::Presets::BODY_SMALL, UIColorPalette::PRIMARY_BLUE);
    
    // Accent colors
    add_styled_text("Accent Yellow", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::ACCENT_YELLOW);
    add_styled_text("Accent Orange", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::ACCENT_ORANGE);
    add_styled_text("Accent Coral", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::ACCENT_CORAL);
    add_styled_text("Accent Green", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::ACCENT_GREEN);
    add_styled_text("Accent Blue", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::ACCENT_BLUE);
    add_styled_text("Accent Amber", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::ACCENT_AMBER);
    
    // Text colors
    add_styled_text("Text Bright", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::TEXT_BRIGHT);
    add_styled_text("Text Primary", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::TEXT_PRIMARY);
    add_styled_text("Text Secondary", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::TEXT_SECONDARY);
    add_styled_text("Text Muted", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::TEXT_MUTED);
    add_styled_text("Text Disabled", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::TEXT_DISABLED);
    
    // Background colors
    add_styled_text("BG Dark (#1B1A20)", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::BG_DARK);
    add_styled_text("BG Medium", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::BG_MEDIUM);
    add_styled_text("BG Light", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::BG_LIGHT);
    add_styled_text("BG Panel", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::BG_PANEL);
    
    // Button colors
    add_styled_text("Button Normal", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::BUTTON_NORMAL);
    add_styled_text("Button Hover", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::BUTTON_HOVER);
    add_styled_text("Button Active", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::BUTTON_ACTIVE);
    add_styled_text("Button Disabled", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::BUTTON_DISABLED);
    
    // Status colors
    add_styled_text("Status Success", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::STATUS_SUCCESS);
    add_styled_text("Status Warning", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::STATUS_WARNING);
    add_styled_text("Status Error", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::STATUS_ERROR);
    add_styled_text("Status Info", UIFontStyles::Presets::BODY_REGULAR, UIColorPalette::STATUS_INFO);
}
