#ifndef UI_COLOR_PALETTE_H
#define UI_COLOR_PALETTE_H

#include <array>

namespace UIColorPalette {
    // Color palette based on provided hex colors
    // Colors are defined as RGBA floats (0.0-1.0)
    // Base colors: #F7D14E (yellow), #FF825C (orange), #88FF9C (green), #3378C7 (blue), #1B1A20 (dark)
    
    // Primary colors - Core brand colors from provided palette
    constexpr std::array<float, 4> PRIMARY_YELLOW = {0.9686f, 0.8196f, 0.3059f, 1.0f};  // #F7D14E - Bright yellow/gold
    constexpr std::array<float, 4> PRIMARY_ORANGE = {1.0f, 0.5098f, 0.3608f, 1.0f};      // #FF825C - Coral orange
    constexpr std::array<float, 4> PRIMARY_GREEN = {0.5333f, 1.0f, 0.6118f, 1.0f};      // #88FF9C - Mint green
    constexpr std::array<float, 4> PRIMARY_BLUE = {0.2f, 0.4706f, 0.7804f, 1.0f};       // #3378C7 - Bright blue
    
    // Accent colors - Variations and complementary colors
    constexpr std::array<float, 4> ACCENT_YELLOW = {0.9686f, 0.8196f, 0.3059f, 1.0f};  // #F7D14E - Same as primary yellow
    constexpr std::array<float, 4> ACCENT_ORANGE = {1.0f, 0.5098f, 0.3608f, 1.0f};      // #FF825C - Same as primary orange
    constexpr std::array<float, 4> ACCENT_CORAL = {1.0f, 0.6f, 0.45f, 1.0f};           // Lighter orange variant
    constexpr std::array<float, 4> ACCENT_GREEN = {0.5333f, 1.0f, 0.6118f, 1.0f};      // #88FF9C - Same as primary green
    constexpr std::array<float, 4> ACCENT_BLUE = {0.2f, 0.4706f, 0.7804f, 1.0f};        // #3378C7 - Same as primary blue
    constexpr std::array<float, 4> ACCENT_AMBER = {0.95f, 0.75f, 0.4f, 1.0f};         // Amber/yellow blend
    
    // Background colors - Dark theme based on #1B1A20
    constexpr std::array<float, 4> BG_DARK = {0.1059f, 0.1020f, 0.1255f, 1.0f};        // #1B1A20 - Base dark background
    constexpr std::array<float, 4> BG_MEDIUM = {0.15f, 0.14f, 0.18f, 1.0f};            // Slightly lighter dark
    constexpr std::array<float, 4> BG_LIGHT = {0.22f, 0.21f, 0.25f, 1.0f};             // Lighter dark
    constexpr std::array<float, 4> BG_PANEL = {0.18f, 0.17f, 0.20f, 1.0f};             // Panel background
    
    // Text colors - Optimized for readability on dark backgrounds
    constexpr std::array<float, 4> TEXT_BRIGHT = {0.9686f, 0.8196f, 0.3059f, 1.0f};   // #F7D14E - Bright yellow for headings
    constexpr std::array<float, 4> TEXT_PRIMARY = {1.0f, 0.5098f, 0.3608f, 1.0f};     // #FF825C - Orange for primary text
    constexpr std::array<float, 4> TEXT_SECONDARY = {0.9f, 0.45f, 0.32f, 1.0f};        // Slightly darker orange
    constexpr std::array<float, 4> TEXT_MUTED = {0.6f, 0.5f, 0.55f, 1.0f};            // Muted gray-purple
    constexpr std::array<float, 4> TEXT_DISABLED = {0.35f, 0.34f, 0.38f, 1.0f};        // Disabled text
    
    // Interactive element colors - Buttons and clickable elements
    constexpr std::array<float, 4> BUTTON_NORMAL = {0.2f, 0.4706f, 0.7804f, 1.0f};     // #3378C7 - Blue for normal buttons
    constexpr std::array<float, 4> BUTTON_HOVER = {0.3f, 0.55f, 0.85f, 1.0f};         // Lighter blue for hover
    constexpr std::array<float, 4> BUTTON_ACTIVE = {0.5333f, 1.0f, 0.6118f, 1.0f};     // #88FF9C - Green for active
    constexpr std::array<float, 4> BUTTON_DISABLED = {0.25f, 0.3f, 0.4f, 1.0f};        // Muted blue-gray
    
    // Border/outline colors - For component borders and dividers
    constexpr std::array<float, 4> BORDER_PRIMARY = {0.9686f, 0.8196f, 0.3059f, 1.0f}; // #F7D14E - Yellow borders
    constexpr std::array<float, 4> BORDER_SECONDARY = {1.0f, 0.5098f, 0.3608f, 1.0f};  // #FF825C - Orange borders
    constexpr std::array<float, 4> BORDER_MUTED = {0.4f, 0.38f, 0.45f, 1.0f};         // Muted border
    
    // Status colors - For alerts, warnings, success
    constexpr std::array<float, 4> STATUS_SUCCESS = {0.5333f, 1.0f, 0.6118f, 1.0f};   // #88FF9C - Green for success
    constexpr std::array<float, 4> STATUS_WARNING = {0.9686f, 0.8196f, 0.3059f, 1.0f}; // #F7D14E - Yellow for warnings
    constexpr std::array<float, 4> STATUS_ERROR = {1.0f, 0.5098f, 0.3608f, 1.0f};     // #FF825C - Orange for errors
    constexpr std::array<float, 4> STATUS_INFO = {0.2f, 0.4706f, 0.7804f, 1.0f};      // #3378C7 - Blue for info

    // Utility: Convert array to individual RGBA values
    inline void get_rgba(const std::array<float, 4>& color, float& r, float& g, float& b, float& a) {
        r = color[0];
        g = color[1];
        b = color[2];
        a = color[3];
    }
}

#endif // UI_COLOR_PALETTE_H

