#include "graphics_components/text_button_component.h"

TextButtonComponent::TextButtonComponent(
    float x, float y, float width, float height, 
    const std::string& text, ButtonCallback callback
) : ButtonComponent(x, y, width, height, callback),
    m_text_component(nullptr)
{
    // Create the text component as a child of this button
    m_text_component = new TextComponent(x, y, width * 0.8f, height * 0.6f, text);
    
    // Add the raw pointer to the GraphicsComponent's children list
    // This transfers ownership to the parent component
    add_child(m_text_component);
}

TextButtonComponent::~TextButtonComponent()
{
    // No need to manually delete m_text_component as it's managed by the parent
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