#ifndef UI_FONT_STYLES_H
#define UI_FONT_STYLES_H

#include <string>

namespace UIFontStyles {
    // Font family names (these should match the font names loaded via TextComponent::load_font)
    
    // Monospace fonts
    constexpr const char* FONT_MONO_REGULAR = "hack_regular";
    constexpr const char* FONT_MONO_BOLD = "hack_bold";
    constexpr const char* FONT_MONO_THIN = "libertad_thin";
    constexpr const char* FONT_MONO_BOLD_ALT = "libertad_bold";
    
    // Display fonts
    constexpr const char* FONT_DISPLAY = "super_smash_tv";
    
    // Default font
    constexpr const char* FONT_DEFAULT = "default";
    
    // Font size presets
    namespace FontSize {
        constexpr int TINY = 16;
        constexpr int SMALL = 24;
        constexpr int REGULAR = 32;
        constexpr int MEDIUM = 48;
        constexpr int LARGE = 64;
        constexpr int XLARGE = 96;
        constexpr int HUGE = 128;
    }
    
    // Font style presets (combinations of font and size)
    struct FontStyle {
        std::string font_name;
        int size;
        
        FontStyle(const std::string& name, int font_size) 
            : font_name(name), size(font_size) {}
    };
    
    // Common font style presets
    namespace Presets {
        const FontStyle HEADING_LARGE(FONT_DISPLAY, FontSize::XLARGE);
        const FontStyle HEADING_MEDIUM(FONT_DISPLAY, FontSize::LARGE);
        const FontStyle HEADING_SMALL(FONT_DISPLAY, FontSize::MEDIUM);
        
        const FontStyle BODY_LARGE(FONT_MONO_REGULAR, FontSize::LARGE);
        const FontStyle BODY_MEDIUM(FONT_MONO_REGULAR, FontSize::MEDIUM);
        const FontStyle BODY_REGULAR(FONT_MONO_REGULAR, FontSize::REGULAR);
        const FontStyle BODY_SMALL(FONT_MONO_REGULAR, FontSize::SMALL);
        
        const FontStyle MONO_BOLD_LARGE(FONT_MONO_BOLD, FontSize::LARGE);
        const FontStyle MONO_BOLD_MEDIUM(FONT_MONO_BOLD, FontSize::MEDIUM);
        const FontStyle MONO_BOLD_REGULAR(FONT_MONO_BOLD, FontSize::REGULAR);
        
        const FontStyle MONO_THIN_REGULAR(FONT_MONO_THIN, FontSize::REGULAR);
        const FontStyle MONO_THIN_SMALL(FONT_MONO_THIN, FontSize::SMALL);
    }
}

#endif // UI_FONT_STYLES_H

