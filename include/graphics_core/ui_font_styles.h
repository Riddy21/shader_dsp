#ifndef UI_FONT_STYLES_H
#define UI_FONT_STYLES_H

#include <string>

namespace UIFontStyles {
    // Font family names (these should match the font names loaded via TextComponent::load_font)
    
    // Monospace fonts
    constexpr const char* LIBERTAD_THIN = "libertad_thin";
    constexpr const char* LIBERTAD_BOLD = "libertad_bold";
    constexpr const char* EIGHT_BIT = "eightbit_regular";
    
    constexpr const char* FONT_DEFAULT = "default";
    
    // Font size presets
    namespace FontSize {
        constexpr int TINY = 16;
        constexpr int SMALL = 24;
        constexpr int MEDIUM = 48;
        constexpr int REGULAR = 32;
        constexpr int LARGE = 64;
    }
    
    // Font style presets (combinations of font and size)
    struct FontStyle {
        std::string font_name;
        int size;
        float h_align;
        float v_align;
        bool antialiased;
        
        FontStyle(const std::string& name, int font_size, float h = 0.5f, float v = 0.5f, bool aa = true) 
            : font_name(name), size(font_size), h_align(h), v_align(v), antialiased(aa) {}
    };
    
    // Common font style presets
    namespace Presets {
        const FontStyle TITLE(EIGHT_BIT, FontSize::TINY, 0.5f, 0.5f, false);
        const FontStyle SUBTITLE(LIBERTAD_THIN, FontSize::REGULAR, 0.5f, 0.5f, true);
    }
}

#endif // UI_FONT_STYLES_H

