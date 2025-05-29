#ifndef TEXT_BUTTON_COMPONENT_H
#define TEXT_BUTTON_COMPONENT_H

#include <memory>
#include <string>
#include "graphics_components/button_component.h"
#include "graphics_components/text_component.h"

class TextButtonComponent : public ButtonComponent {
public:
    TextButtonComponent(
        float x, float y, float width, float height, 
        const std::string& text, ButtonCallback callback,
        EventHandler* event_handler = nullptr,
        const RenderContext& render_context = RenderContext()
    );
    ~TextButtonComponent() override;

    // Text-specific methods
    void set_text(const std::string& text);
    const std::string& get_text() const;
    
    void set_text_color(float r, float g, float b, float a);
    void set_hover_text_color(float r, float g, float b, float a);
    void set_active_text_color(float r, float g, float b, float a);
    
    void set_font_size(int size);
    void set_horizontal_alignment(float alignment);
    void set_vertical_alignment(float alignment);
    bool set_font(const std::string& font_name);
    
    // Override to update text component based on button state
    void update_children() override;

private:
    TextComponent* m_text_component;
    
    // Store text colors for different states
    float m_text_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float m_hover_text_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float m_active_text_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

#endif // TEXT_BUTTON_COMPONENT_H