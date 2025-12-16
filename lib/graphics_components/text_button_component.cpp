#include "graphics_components/text_button_component.h"

TextButtonComponent::TextButtonComponent(
    float x, float y, float width, float height, 
    const std::string& text, ButtonCallback callback
) : ButtonComponent(x, y, width, height, callback),
    m_text_component(nullptr)
{
    // Create the text component as a child of this button
    m_text_component = new TextComponent(x, y, width, height, text);
    
    // Add the raw pointer to the GraphicsComponent's children list
    // This transfers ownership to the parent component
    add_child(m_text_component);
}

TextButtonComponent::TextButtonComponent(
    float x, float y, float width, float height, 
    const std::string& text, const UIButtonStyle& style, const std::array<float, 4>& color, ButtonCallback callback
) : ButtonComponent(x, y, width, height, callback),
    m_text_component(nullptr)
{
    // Create the text component as a child of this button
    m_text_component = new TextComponent(x, y, width, height, text);
    
    // Add the raw pointer to the GraphicsComponent's children list
    // This transfers ownership to the parent component
    add_child(m_text_component);
    
    // Apply style
    set_style(style, color);
}

TextButtonComponent::~TextButtonComponent()
{
    // No need to manually delete m_text_component as it's managed by the parent
}

void TextButtonComponent::set_style(const UIButtonStyle& style, const std::array<float, 4>& color) {
    // Helper to create a color with specific alpha
    auto with_alpha = [](const std::array<float, 4>& c, float alpha) -> float* {
        static float result[4];
        result[0] = c[0]; result[1] = c[1]; result[2] = c[2]; result[3] = alpha;
        return result;
    };

    // Apply background colors based on style alpha and base color
    set_colors(color[0], color[1], color[2], style.normal_bg_alpha);
    set_hover_colors(color[0], color[1], color[2], style.hover_bg_alpha);
    set_active_colors(color[0], color[1], color[2], style.active_bg_alpha);
    
    // Apply border properties
    set_border_color(color[0], color[1], color[2], color[3]); // Border uses full alpha
    set_border_width(style.border_width);
    set_border_visible(style.show_border);
    
    // Apply text properties
    set_font(style.font_style.font_name);
    set_font_size(style.font_style.size);
    
    // Set text colors
    // Use the base color for text
    set_text_color(color[0], color[1], color[2], color[3]);
    set_hover_text_color(color[0], color[1], color[2], color[3]);
    
    // Active text - maybe black/dark if background is bright?
    // For now, let's keep it black on active to match previous request "when selected... darker"
    // Actually, "hover and selected brighter". If active BG is 0.7 alpha (bright), dark text makes sense.
    // Let's use BG_DARK for active text to ensure contrast
    // Hardcoded for now as per previous behavior
    set_active_text_color(0.1f, 0.1f, 0.12f, 1.0f);
}

void TextButtonComponent::set_text(const std::string& text) {
    if (m_text_component) {
        m_text_component->set_text(text);
    }
}

const std::string& TextButtonComponent::get_text() const {
    static const std::string empty_string;
    return m_text_component ? m_text_component->get_text() : empty_string;
}

void TextButtonComponent::set_text_color(float r, float g, float b, float a) {
    m_text_color[0] = r;
    m_text_color[1] = g;
    m_text_color[2] = b;
    m_text_color[3] = a;
    
    if (m_text_component) {
        m_text_component->set_text_color(r, g, b, a);
    }
}

void TextButtonComponent::set_hover_text_color(float r, float g, float b, float a) {
    m_hover_text_color[0] = r;
    m_hover_text_color[1] = g;
    m_hover_text_color[2] = b;
    m_hover_text_color[3] = a;
    
    // Apply immediately if the button is currently hovered
    if (is_hovered() && m_text_component) {
        m_text_component->set_text_color(r, g, b, a);
    }
}

void TextButtonComponent::set_active_text_color(float r, float g, float b, float a) {
    m_active_text_color[0] = r;
    m_active_text_color[1] = g;
    m_active_text_color[2] = b;
    m_active_text_color[3] = a;
    
    // Apply immediately if the button is currently pressed
    if (is_pressed() && m_text_component) {
        m_text_component->set_text_color(r, g, b, a);
    }
}

void TextButtonComponent::set_font_size(int size) {
    if (m_text_component) {
        m_text_component->set_font_size(size);
    }
}

void TextButtonComponent::set_horizontal_alignment(float alignment) {
    if (m_text_component) {
        m_text_component->set_horizontal_alignment(alignment);
    }
}

void TextButtonComponent::set_vertical_alignment(float alignment) {
    if (m_text_component) {
        m_text_component->set_vertical_alignment(alignment);
    }
}

bool TextButtonComponent::set_font(const std::string& font_name) {
    return m_text_component ? m_text_component->set_font(font_name) : false;
}

void TextButtonComponent::update_children() {
    // Update text color based on button state
    if (!m_text_component) return;
    
    if (is_pressed()) {
        m_text_component->set_text_color(
            m_active_text_color[0], 
            m_active_text_color[1], 
            m_active_text_color[2], 
            m_active_text_color[3]
        );
    } else if (is_hovered()) {
        m_text_component->set_text_color(
            m_hover_text_color[0], 
            m_hover_text_color[1], 
            m_hover_text_color[2], 
            m_hover_text_color[3]
        );
    } else {
        m_text_component->set_text_color(
            m_text_color[0], 
            m_text_color[1], 
            m_text_color[2], 
            m_text_color[3]
        );
    }
}